/* Author: Nick Downing downing.nick@gmail.com
 * License: GPLv3, see LICENSE in repository root
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>

uint16_t VENDOR = 0x1d50;
uint16_t PRODUCT = 0x6018;

int main() {
  int res = libusb_init(0);
  if (res < 0) {
    fprintf(stderr, "libusb_init: %s\n", libusb_strerror(res));
    exit(1);
  }

  while (1) {
    libusb_device **devs;
    ssize_t cnt = libusb_get_device_list(0, &devs);
    if (cnt < 0) {
      fprintf(stderr, "libusb_get_device_list: %s\n", libusb_strerror(cnt));
      exit(1);
    }

    /*printf("Trying to find Black Magic probe.\n");*/
    int i;
    for (i = 0; devs[i]; ++i) {
      struct libusb_device_descriptor desc;
      int res = libusb_get_device_descriptor(devs[i], &desc);
      if (res < 0) {
        fprintf(stderr, "libusb_get_device_descriptor: %s\n", libusb_strerror(res));
        exit(1);
      }
      /*printf("%04x:%04x (bus %d, device %d)\n", desc.idVendor, desc.idProduct, libusb_get_bus_number(devs[i]), libusb_get_device_address(devs[i]));*/

      if (desc.idVendor == VENDOR && desc.idProduct == PRODUCT)
        goto found;
    }
    fprintf(stderr, "Black Magic probe not found.\n");
    goto lost_device;

found:
    /*printf("Found Black Magic probe.\n")*/;
    libusb_device_handle *handle;
    res = libusb_open(devs[i], &handle);
    libusb_free_device_list(devs, 1);
    if (res < 0) {
      fprintf(stderr, "libusb_open: %s\n", libusb_strerror(res));
      if (res == LIBUSB_ERROR_NO_DEVICE)
        goto lost_device;
      exit(1);
    }

    //printf("opened");

    res = libusb_claim_interface(handle, 5); // Black Magic Trace Capture
    if (res < 0) {
      fprintf(stderr, "libusb_claim_interface: %s\n", libusb_strerror(res));
      if (res == LIBUSB_ERROR_NO_DEVICE)
        goto lost_device;
      exit(1);
    }

    //printf("claimed\n");

    unsigned char ENDPOINT_UP = 0x85;
    int count;
    int data_bytes = 0;
    int chan = 0;
    unsigned char dataUp[64];
    while (1) {
      //fprintf(stderr,"waiting for bulk transfer\n");
      res = libusb_bulk_transfer(handle, ENDPOINT_UP, dataUp, sizeof(dataUp), &count, 0);
      //fprintf(stderr, "got bulk transfer, res: %i, count: %i\n", res, count);
      if (res < 0) {
        fprintf(stderr, "libusb_bulk_transfer: %s\n", libusb_strerror(res));
        if (res == LIBUSB_ERROR_NO_DEVICE)
          goto lost_device;
        continue;
      }
      for (int j = 0; j < count; j += 1) {
        if(data_bytes != 0) {
          printf("%c", dataUp[j]);
          data_bytes--;
        } else {
          int newchan = dataUp[j] >> 3;
          if(chan != newchan) {
            printf("\n%i: ", newchan);
            chan = newchan;
          }
          switch(dataUp[j] & 3) {
            case 0: goto bad_packet;
            case 1:
              data_bytes = 1;
              break;
            case 2:
              data_bytes = 2;
              break;
            case 3:
              data_bytes = 4;
              break;
          }
        }
      }
      continue;
    bad_packet:
      printf("\n");
      fprintf(stderr, "bad packet:");
      for(int j = 0; j < count; j++)
        fprintf(stderr, " %02x",dataUp[j]);
      fprintf(stderr, "\n");
    }
    // can never get here
  lost_device:
    sleep(1);
  }
}
