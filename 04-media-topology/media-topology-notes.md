# Phase 4 — Media/Display Topology

## What `of_graph` Solves

Hardware media pipelines connect multiple chips — a camera sensor feeds an ISP, which feeds a display engine, which feeds a panel or HDMI bridge. Each chip is a separate DT node. `of_graph` is the DT convention for describing directed connections between these nodes.

Without `of_graph`, drivers had no standard way to discover which node is upstream or downstream. `of_graph` provides:
- A standard `port` / `endpoint` / `remote-endpoint` schema
- Helper APIs (`of_graph_get_remote_endpoint`, `v4l2_async_subdev`, etc.) that drivers use to walk the pipeline at probe time

---

## `of_graph` Building Blocks

### Nodes

| Node | Role |
|---|---|
| `port` | One physical interface (e.g. MIPI CSI-2 output pad) |
| `endpoint` | One connection within a port — a port can have multiple endpoints for mux scenarios |
| `remote-endpoint` | Phandle pointing to the peer endpoint on the other chip |

### Minimal two-node pipeline

```dts
/* Camera sensor */
ov5647: camera@36 {
    compatible = "ovti,ov5647";
    reg = <0x36>;

    port {
        ov5647_out: endpoint {
            remote-endpoint = <&csi1_ep>;   /* points to ISP input */
            clock-lanes = <0>;
            data-lanes = <1 2>;
        };
    };
};

/* MIPI CSI-2 receiver (ISP input) */
csi1: csi@7e5b0000 {
    compatible = "brcm,bcm2835-unicam";
    reg = <0x7e5b0000 0x400>;

    port {
        csi1_ep: endpoint {
            remote-endpoint = <&ov5647_out>;  /* points back to sensor */
            clock-lanes = <0>;
            data-lanes = <1 2>;
        };
    };
};
```

`remote-endpoint` phandles are symmetric — each side points to the other.

---

## Multi-Port Node (Display Bridge Example)

A display bridge chip (e.g. DSI-to-HDMI) has two ports: one input from the SoC DSI controller, one output to the HDMI connector.

```dts
/* SoC DSI controller */
dsi: dsi@fe700000 {
    compatible = "brcm,bcm2711-dsi1";
    reg = <0x0 0xfe700000 0x0 0x8c>;
    #address-cells = <1>;
    #size-cells = <0>;

    port {
        dsi_out: endpoint {
            remote-endpoint = <&bridge_in>;
        };
    };
};

/* DSI-to-HDMI bridge (e.g. TC358743) */
bridge: hdmi-bridge@0f {
    compatible = "toshiba,tc358743";
    reg = <0x0f>;

    ports {
        #address-cells = <1>;
        #size-cells = <0>;

        port@0 {                        /* input: from DSI */
            reg = <0>;
            bridge_in: endpoint {
                remote-endpoint = <&dsi_out>;
            };
        };

        port@1 {                        /* output: to HDMI connector */
            reg = <1>;
            bridge_out: endpoint {
                remote-endpoint = <&hdmi_con_in>;
            };
        };
    };
};

/* HDMI connector */
hdmi_con: connector {
    compatible = "hdmi-connector";
    type = "a";

    port {
        hdmi_con_in: endpoint {
            remote-endpoint = <&bridge_out>;
        };
    };
};
```

Full pipeline:
```
dsi@fe700000 → TC358743 bridge → HDMI connector
 (dsi_out)      (bridge_in → bridge_out)   (hdmi_con_in)
```

---

## How V4L2 Drivers Consume `of_graph`

### Async subdev registration (camera side)

The V4L2 async framework lets the ISP driver register itself and then wait for subdevices (sensors) to become available, regardless of probe order.

```c
#include <media/v4l2-async.h>

/* In ISP probe: parse all remote endpoints from DT */
static int unicam_probe(struct platform_device *pdev)
{
    struct device_node *ep;
    struct v4l2_async_subdev *asd;

    ep = of_graph_get_next_endpoint(pdev->dev.of_node, NULL);
    while (ep) {
        struct device_node *remote = of_graph_get_remote_port_parent(ep);

        asd = v4l2_async_notifier_add_fwnode_subdev(
                &priv->notifier,
                of_fwnode_handle(remote),
                sizeof(*asd));

        of_node_put(remote);
        ep = of_graph_get_next_endpoint(pdev->dev.of_node, ep);
    }

    return v4l2_async_notifier_register(&priv->v4l2_dev, &priv->notifier);
}
```

When the sensor driver probes later, V4L2 calls the notifier's `.bound()` callback — this is where the ISP links its media entity to the sensor's entity.

### Key `of_graph` API

```c
/* Walk all endpoints of a node */
of_graph_get_next_endpoint(node, prev)

/* From an endpoint, get the node at the other end */
of_graph_get_remote_port_parent(endpoint)

/* Get the remote endpoint node itself (not its parent) */
of_graph_get_remote_endpoint(endpoint)

/* Parse port number from a port node */
of_graph_get_port_by_id(node, id)
```

All returned `device_node *` values need `of_node_put()` when done.

---

## How DRM Drivers Consume `of_graph`

DRM (display) drivers use `drm_of_find_panel_or_bridge()` to locate the downstream panel or bridge from the encoder's output port.

```c
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include <drm/drm_bridge.h>

static int dsi_probe(struct platform_device *pdev)
{
    struct drm_panel *panel = NULL;
    struct drm_bridge *bridge = NULL;

    ret = drm_of_find_panel_or_bridge(pdev->dev.of_node,
                                       1,    /* port index */
                                       0,    /* endpoint index */
                                       &panel, &bridge);
    if (ret)
        return ret;

    if (panel) {
        /* attach panel directly */
    } else if (bridge) {
        drm_bridge_attach(encoder, bridge, NULL, 0);
    }
}
```

`drm_of_find_panel_or_bridge` walks the `of_graph` from the given port/endpoint, finds the remote node, and checks if it matches a registered `drm_panel` or `drm_bridge`.

---

## Annotated Real Pipeline: Raspberry Pi 4 Display

The Pi 4 display pipeline for the official 7" DSI touchscreen:

```
bcm2711 SoC
  └── dsi1 (DSI controller, fe700000)
        └── port → endpoint → remote: panel (ili9881)
                                         └── port → endpoint → remote: dsi1
```

In the DTS (`bcm2711-rpi-4-b.dts`):
```dts
&dsi1 {
    status = "okay";
    #address-cells = <1>;
    #size-cells = <0>;

    port {
        dsi1_out_port: endpoint {
            remote-endpoint = <&panel_dsi_port>;
        };
    };
};

/* Applied via overlay: vc4-kms-dsi-7inch.dts */
fragment@1 {
    target = <&dsi1>;
    __overlay__ {
        panel: panel@0 {
            compatible = "raspberrypi,7inch-touchscreen-panel";
            reg = <0>;

            port {
                panel_dsi_port: endpoint {
                    remote-endpoint = <&dsi1_out_port>;
                };
            };
        };
    };
};
```

Probe sequence:
1. `bcm2835-dsi` driver probes `dsi1`, calls `drm_of_find_panel_or_bridge` on port 0
2. Panel driver not yet probed → deferred (`-EPROBE_DEFER`)
3. Panel driver probes, registers `drm_panel`
4. DRM core retries `dsi1` probe → `drm_of_find_panel_or_bridge` succeeds → `drm_panel_attach`

---

## Inspecting a Pipeline on the Pi

```bash
# List all media devices
ls /dev/media*

# Show the media topology (requires v4l-utils)
sudo apt install v4l-utils
media-ctl -d /dev/media0 --print-topology

# List DRM connectors
for c in /sys/class/drm/card*-*; do
    echo "$c: $(cat $c/status)"
done

# Show of_graph endpoints in live DT
find /proc/device-tree -name "remote-endpoint" | while read f; do
    echo "$(dirname $f | sed 's|/proc/device-tree||')"
done
```

---

## Common `of_graph` Mistakes

| Mistake | Symptom |
|---|---|
| `remote-endpoint` phandles not symmetric | One side probes, other side defers forever |
| Wrong port `reg` index | `drm_of_find_panel_or_bridge` returns `ENODEV` |
| Missing `#address-cells`/`#size-cells` on `ports` node | `dtc` warning; port `reg` not parsed |
| `of_node_put` missing after `of_graph_get_remote_port_parent` | `of_node` refcount leak, warning at shutdown |

---

## References

- [of_graph kernel docs](https://www.kernel.org/doc/html/latest/driver-api/device_link.html)
- [V4L2 async subdev](https://www.kernel.org/doc/html/latest/userspace-api/media/v4l/dev-subdev.html)
- [DRM bridge/panel API](https://www.kernel.org/doc/html/latest/gpu/drm-kms-helpers.html)
- [bcm2711-rpi-4-b.dts](https://elixir.bootlin.com/linux/latest/source/arch/arm64/boot/dts/broadcom/bcm2711-rpi-4-b.dts)
- [Raspberry Pi DSI overlay](https://github.com/raspberrypi/linux/blob/rpi-6.6.y/arch/arm/boot/dts/overlays/vc4-kms-dsi-7inch.dts)
