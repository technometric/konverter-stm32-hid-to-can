#ifndef __USBD_EP_CONF_H
#define __USBD_EP_CONF_H

#ifdef USBCON

#include <stdint.h>
#include "usbd_def.h"

typedef struct {
  uint32_t ep_adress;
  uint32_t ep_size;
#if defined(USB)
  uint32_t ep_kind;
#endif
} ep_desc_t;

#ifdef USBD_USE_HID_COMPOSITE
  #define HID_MOUSE_EPIN_ADDR           0x81U
  #define HID_KEYBOARD_EPIN_ADDR        0x82U

  #define HID_MOUSE_EPIN_SIZE           64U
  #define HID_KEYBOARD_EPIN_SIZE        64U

  #define DEV_NUM_EP                    0x03U
#endif

#if defined(USB)
  #define PMA_EP0_OUT_ADDR              (8 * DEV_NUM_EP)
  #define PMA_EP0_IN_ADDR               (PMA_EP0_OUT_ADDR + USB_MAX_EP0_SIZE)
  #ifdef USBD_USE_HID_COMPOSITE
    #define PMA_MOUSE_IN_ADDR           (PMA_EP0_IN_ADDR + HID_MOUSE_EPIN_SIZE)
    #define PMA_KEYBOARD_IN_ADDR        (PMA_MOUSE_IN_ADDR + HID_KEYBOARD_EPIN_SIZE)
  #endif
#endif

extern const ep_desc_t ep_def[DEV_NUM_EP + 1];

#endif
#endif
