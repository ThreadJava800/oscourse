#include <drivers/virtio/virtio_net.h>
#include <inc/lib.h>

VirtioNetDevice global_nic;

int
virtio_module_init() {
    PciDevice *pci_nic = find_pci_dev(
            VIRTIO_PCI_VENDOR_ID,
            VIRTIO_PCI_DEVICE_ID_NETWORK);
    if (pci_nic != NULL) {
        cprintf("%s: Found virtio network card %p\n", __func__, pci_nic);
        int err = virtio_net_init(&global_nic, pci_nic);
        if (err != 0) {
            cprintf("%s: Unable to init found network card\n", __func__);
            return err;
        }
        cprintf("%s: Success init\n", __func__);
    }

    return 0;
}

VirtioNetDevice *get_virtio_net_dev() {
    return &global_nic;
}
