# fty-info

fty-info is an agent providing IPM2 system information.

## How to build

To build fty-info project run:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=usr -DBUILD_TESTING=On ..
make
sudo make install
```

Optionally, to run fty-outage tests run:

```bash
make test
```

## How to run

To run fty-info project:

* from within the source tree, run:

```bash
./src/fty-info
```

For the other options available, refer to the manual page of fty-info.

* from an installed base, using systemd, run:

```bash
systemctl start fty-info
```

### Configuration file

Agent has a configuration file: fty-info.cfg.
Except standard server and malamute options, there are two other options:
* server/check_interval for how often to publish Linux system metrics
* parameters/path for REST API root used by IPM Infra software
Agent reads environment variable BIOS_LOG_LEVEL, which sets verbosity level of the agent.

## Architecture

### Overview

fty-info is composed of 2 actors:

* info-server: processes raw data to get RC information and distributes it further
* info-rc0-runonce: on start, puts the gathered RC data into DB

In addition to actors, there is one timer:

* linuxmetrics timer: runs every linuxmetrics_interval (by default every 30 seconds) and triggers publication of Linux system metrics

## Protocols

### Published metrics

Agent publishes Linux system metrics on FTY_PROTO_STREAM_METRICS, for example:

```bash
stream=METRICS
sender=fty_info_linuxmetrics
subject=usage.memory@rackcontroller-0
D: 17-10-17 06:34:24 FTY_PROTO_METRIC:
D: 17-10-17 06:34:24     aux=
D: 17-10-17 06:34:24     time=1508222064
D: 17-10-17 06:34:24     ttl=90
D: 17-10-17 06:34:24     type='usage.memory'
D: 17-10-17 06:34:24     name='rackcontroller-0'
D: 17-10-17 06:34:24     value='40.000000'
D: 17-10-17 06:34:24     unit='%'
```

### Published alerts

Agent doesn't publish any alerts.

### Mailbox requests

It is possible to request the fty-info agent for:

* RC information
* HW Capability

#### RC information

The USER peer (supposed to be REST API) sends the following messages using MAILBOX SEND to
FTY-INFO-AGENT ("fty-info") peer:

* INFO/'msg-correlation-id'

where:

* 'msg-correlation-id' MUST be zuuid identifier provided by USER
* subject of the message MUST be discarded

The FTY-INFO-AGENT peer MUST respond with one of the messages back to USER
peer using MAILBOX SEND.

* 'msg-correlation-id'/INFO/'srv-name'/'srv-type'/'srv-subtype'/'srv-port'/'info-hash'

where
* '/' indicates a multipart frame message
* subject of the message MUST be discarded
* 'msg-correlation-id' MUST be the same as in request
* 'srv-name' MUST be IPC ('short-uuid'), where 'short-uuid' is first 16 bytes of RC UUID
* 'srv-type' MUST be service type recognized by mDNS (for example \_https.\_tcp.)
* 'srv-subtype' MUST be service subtype recognized by mDNS (for example \_powerservice.\_sub.\_https.\_tcp.)
* 'srv-port' MUST be network port on which the service is available (for example 443)
* 'info-hash' MUST be zframe containing hash with the following keys:
    * "id"
    * "uuid"
    * "hostname"
    * "name"
    * "name-uri"
    * "vendor"
    * "manufacturer"
    * "product"
    * "serialNumber"
    * "partNumber"
    * "location"
    * "parent-uri"
    * "version"
    * "description"
    * "contact"
    * "installDate"
    * "path"
    * "protocol-format"
    * "type"
    * "txtvers"
    * "ip.1"
    * "ip.2"
    * "ip.3"

    Value associated with ANY key MAY be NULL.

Example `bmsg` request:
```bash
bmsg request fty-info REQUEST INFO 1234
```

#### HW Capability Request

* HW_CAP/'msg-correlation-id'/'type'

where:

* 'type' can be gpi, gpo, type

Response of FTY_INFO:

* 'msg-correlation-id'/OK/'type'/'count'/'base_address'/'offset'/'mapping1'/'mapping_val1'/'mapping2'/'mapping_val2'/ ...
* 'msg-correlation-id'/ERROR/'reason'

where:

* 'count' - number of GPI/GPO pins
* 'offset' - offset of pin numbering (GPI pins have -1 offset, i.e. GPI 1 is pin 0, ... )
* 'mapping' - Mapping between GPI/GPO number and HW pin number

Example `bmsg` request:
```bash
bmsg request fty-info REQUEST HW_CAP 1234 gpi
bmsg request fty-info REQUEST HW_CAP 1234 gpo
```

### Stream subscriptions

* Actor info-server is subscribed to ASSETS stream. On receiving such a message, it MUST:
    * update cache with location topology information
    * republish INFO message on ASSET stream IF the ASSET message was CREATE or UPDATE of this RC

    Detection is based on equality of IP address.

* Actor info-rc0-runonce is subscribed to ASSETS stream, but only for rackcontroller-0 UPDATE messages. On receiving first such a message, it MUST:
    * use all the information provided in the message to update stored RC info
    * re-send the received ASSET message as ASSET_MANIPULATION message to FTY-ASSET-AGENT (asset-agent)

    On receiving next such a message, or any other message, it MUST do nothing.
