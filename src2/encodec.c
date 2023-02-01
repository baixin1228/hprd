#include <stdlib.h>
#include <unistd.h>

#include "util.h"
#include "dl_help.h"
#include "encodec.h"
#include "frame_buffer.h"
#include "dev_templete.h"

DEV_SET_INFO(encodec, encodec_ops)
DEV_MAP_FB(encodec, encodec_ops)
DEV_UNMAP_FB(encodec, encodec_ops)
DEV_GET_FB(encodec, encodec_ops)
DEV_PUT_FB(encodec, encodec_ops)
DEV_RELEASE(encodec, encodec_ops)