#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <math.h>
#include <va/va.h>
#include <va/va_enc_hevc.h>

#include "util.h"
#include "va_display.h"
#include "codec/encodec.h"

#define CHECK_VASTATUS(va_status,func)                                  \
    if (va_status != VA_STATUS_SUCCESS) {                               \
        func_error("failed,exit");                                      \
        exit(1);                                                        \
    }

#define NAL_REF_IDC_NONE        0
#define NAL_REF_IDC_LOW         1
#define NAL_REF_IDC_MEDIUM      2
#define NAL_REF_IDC_HIGH        3

#define FRAME_I 1
#define FRAME_P 2
#define FRAME_B 3
#define FRAME_IDR 7

// SLICE TYPE HEVC ENUM
enum {
    SLICE_B = 0,
    SLICE_P = 1,
    SLICE_I = 2,
};
#define IS_I_SLICE(type) (SLICE_I == (type))
#define IS_P_SLICE(type) (SLICE_P == (type))
#define IS_B_SLICE(type) (SLICE_B == (type))



#define ENTROPY_MODE_CAVLC      0
#define ENTROPY_MODE_CABAC      1

#define PROFILE_IDC_MAIN        1
#define PROFILE_IDC_MAIN10      2

#define BITSTREAM_ALLOCATE_STEPPING     4096
static  int LCU_SIZE = 32;

enum NALUType {
    NALU_TRAIL_N        = 0x00, // Coded slice segment of a non-TSA, non-STSA trailing picture - slice_segment_layer_rbsp, VLC
    NALU_TRAIL_R        = 0x01, // Coded slice segment of a non-TSA, non-STSA trailing picture - slice_segment_layer_rbsp, VLC
    NALU_TSA_N          = 0x02, // Coded slice segment of a TSA picture - slice_segment_layer_rbsp, VLC
    NALU_TSA_R          = 0x03, // Coded slice segment of a TSA picture - slice_segment_layer_rbsp, VLC
    NALU_STSA_N         = 0x04, // Coded slice of an STSA picture - slice_layer_rbsp, VLC
    NALU_STSA_R         = 0x05, // Coded slice of an STSA picture - slice_layer_rbsp, VLC
    NALU_RADL_N         = 0x06, // Coded slice of an RADL picture - slice_layer_rbsp, VLC
    NALU_RADL_R         = 0x07, // Coded slice of an RADL picture - slice_layer_rbsp, VLC
    NALU_RASL_N         = 0x08, // Coded slice of an RASL picture - slice_layer_rbsp, VLC
    NALU_RASL_R         = 0x09, // Coded slice of an RASL picture - slice_layer_rbsp, VLC
    /* 0x0a..0x0f - Reserved */
    NALU_BLA_W_LP       = 0x10, // Coded slice segment of an BLA picture - slice_segment_layer_rbsp, VLC
    NALU_BLA_W_DLP      = 0x11, // Coded slice segment of an BLA picture - slice_segment_layer_rbsp, VLC
    NALU_BLA_N_LP       = 0x12, // Coded slice segment of an BLA picture - slice_segment_layer_rbsp, VLC
    NALU_IDR_W_DLP      = 0x13, // Coded slice segment of an IDR picture - slice_segment_layer_rbsp, VLC
    NALU_IDR_N_LP       = 0x14, // Coded slice segment of an IDR picture - slice_segment_layer_rbsp, VLC
    NALU_CRA            = 0x15, // Coded slice segment of an CRA picture - slice_segment_layer_rbsp, VLC
    /* 0x16..0x1f - Reserved */
    NALU_VPS            = 0x20, // Video parameter set - video_parameter_set_rbsp, non-VLC
    NALU_SPS            = 0x21, // Sequence parameter set - seq_parameter_set_rbsp, non-VLC
    NALU_PPS            = 0x22, // Picture parameter set - pic_parameter_set_rbsp, non-VLC
    NALU_AUD            = 0x23, // Access unit delimiter - access_unit_delimiter_rbsp, non-VLC
    NALU_EOS            = 0x24, // End of sequence - end_of_seq_rbsp, non-VLC
    NALU_EOB            = 0x25, // End of bitsteam - end_of_bitsteam_rbsp, non-VLC
    NALU_FD             = 0x26, // Filler data - filler_data_rbsp, non-VLC
    NALU_PREFIX_SEI     = 0x27, // Supplemental enhancement information (SEI) - sei_rbsp, non_VLC
    NALU_SUFFIX_SEI     = 0x28, // Supplemental enhancement information (SEI) - sei_rbsp, non_VLC
    /* 0x29..0x2f - Reserved */
    /* 0x30..0x3f - Unspecified */
    //this should be the last element of this enum
    //chagne this value if NAL unit type increased
    MAX_HEVC_NAL_TYPE   = 0x3f,

};

// Config const values
#define MAX_TEMPORAL_SUBLAYERS         8
#define MAX_LAYER_ID                   64
#define MAX_LONGTERM_REF_PIC           32
#define NUM_OF_EXTRA_SLICEHEADER_BITS  3
struct ProfileTierParamSet {
    uint8_t      general_profile_space;                                        //u(2)
    int          general_tier_flag;                                            //u(1)
    uint8_t      general_profile_idc;                                          //u(5)
    int          general_profile_compatibility_flag[32];                       //u(1)
    int          general_progressive_source_flag;                              //u(1)
    int          general_interlaced_source_flag;                               //u(1)
    int          general_non_packed_constraint_flag;                           //u(1)
    int          general_frame_only_constraint_flag;                           //u(1)
    int          general_reserved_zero_43bits[43];                             //u(1)
    int          general_reserved_zero_bit;                                    //u(1)
    uint8_t      general_level_idc;                                            //u(8)
};
// Video parameter set structure
struct VideoParamSet {
    uint8_t       vps_video_parameter_set_id;                                   //u(4)
    int           vps_base_layer_internal_flag;                                 //u(1)
    int           vps_base_layer_available_flag;                                //u(1)
    uint8_t       vps_max_layers_minus1;                                        //u(6)
    uint8_t       vps_max_sub_layers_minus1;                                    //u(3)
    int           vps_temporal_id_nesting_flag;                                 //u(1)
    uint16_t      vps_reserved_0xffff_16bits;                                   //u(16)

    struct        ProfileTierParamSet ptps;
    uint8_t       vps_max_nuh_reserved_zero_layer_id;
    uint32_t      vps_max_op_sets;
    uint32_t      vps_num_op_sets_minus1;

    int           vps_sub_layer_ordering_info_present_flag;                     //u(1)
    uint32_t      vps_max_dec_pic_buffering_minus1[MAX_TEMPORAL_SUBLAYERS];      //ue(v)
    uint32_t      vps_max_num_reorder_pics[MAX_TEMPORAL_SUBLAYERS];              //ue(v)
    uint32_t      vps_max_latency_increase_plus1[MAX_TEMPORAL_SUBLAYERS];        //ue(v)
    uint8_t       vps_max_layer_id;                                             //u(6)
    uint32_t      vps_num_layer_sets_minus1;                                    //ue(v)
    int           layer_id_included_flag[MAX_TEMPORAL_SUBLAYERS][MAX_LAYER_ID];   //u(1)
    int           vps_timing_info_present_flag;                                 //u(1)
    uint32_t      vps_num_units_in_tick;                                        //u(32)
    uint32_t      vps_time_scale;                                               //u(32
    int           vps_poc_proportional_to_timing_flag;                          //u(1)
    uint32_t      vps_num_ticks_poc_diff_one_minus1;                            //ue(v)
    uint32_t      vps_num_hrd_parameters;                                       //ue(v)
    uint32_t      hrd_layer_set_idx[MAX_TEMPORAL_SUBLAYERS];                     //ue(v)
    int           cprms_present_flag[MAX_TEMPORAL_SUBLAYERS];                    //u(1)
    int           vps_extension_flag;                                           //u(1)
    int           vps_extension_data_flag;                                      //u(1)
};

struct ShortTermRefPicParamSet {
    int         inter_ref_pic_set_prediction_flag;                               //u(1)
    uint32_t    delta_idx_minus1;                                               //ue(v)
    uint8_t     delta_rps_sign;                                                 //u(1)
    uint32_t    abs_delta_rps_minus1;                                           //ue(v)
    uint8_t     used_by_curr_pic_flag[32];                                      //u(1)
    uint8_t     use_delta_flag[32];                                             //u(1)
    uint32_t    num_negative_pics;                                              //ue(v)
    uint32_t    num_positive_pics;                                              //ue(v)
    uint32_t    delta_poc_s0_minus1[32];                                        //ue(v)
    uint8_t     used_by_curr_pic_s0_flag[32];                                   //u(1)
    uint32_t    delta_poc_s1_minus1[32];                                        //ue(v)
    uint8_t     used_by_curr_pic_s1_flag[32];                                   //u(1)
};
struct SeqParamSet {
    uint8_t     sps_video_parameter_set_id;                                     //u(4)
    uint8_t     sps_max_sub_layers_minus1;                                      //u(3)
    int         sps_temporal_id_nesting_flag;                                   //u(1)

    struct      ProfileTierParamSet ptps;
    uint32_t    sps_seq_parameter_set_id;                                       //ue(v)
    uint32_t    chroma_format_idc;                                              //ue(v)
    int         separate_colour_plane_flag;                                     //u(1)
    uint32_t    pic_width_in_luma_samples;                                      //ue(v)
    uint32_t    pic_height_in_luma_samples;                                     //ue(v)
    int         conformance_window_flag;                                        //u(1)
    uint32_t    conf_win_left_offset;                                           //ue(v)
    uint32_t    conf_win_right_offset;                                          //ue(v)
    uint32_t    conf_win_top_offset;                                            //ue(v)
    uint32_t    conf_win_bottom_offset;                                         //ue(v)
    uint32_t    bit_depth_luma_minus8;                                          //ue(v)
    uint32_t    bit_depth_chroma_minus8;                                        //ue(v)
    uint32_t    log2_max_pic_order_cnt_lsb_minus4;                              //ue(v)
    int         sps_sub_layer_ordering_info_present_flag;                       //u(1)
    uint32_t    sps_max_dec_pic_buffering_minus1[MAX_TEMPORAL_SUBLAYERS];        //ue(v)
    uint32_t    sps_max_num_reorder_pics[MAX_TEMPORAL_SUBLAYERS];                //ue(v)
    uint32_t    sps_max_latency_increase_plus1[MAX_TEMPORAL_SUBLAYERS];          //ue(v)
    uint32_t    log2_min_luma_coding_block_size_minus3;                         //ue(v)
    uint32_t    log2_diff_max_min_luma_coding_block_size;
    uint32_t    log2_max_coding_block_size_minus3; //ue(v)
    uint32_t    log2_min_luma_transform_block_size_minus2;                      //ue(v)
    uint32_t    log2_diff_max_min_luma_transform_block_size;                    //ue(v)
    uint32_t    max_transform_hierarchy_depth_inter;                            //ue(v)
    uint32_t    max_transform_hierarchy_depth_intra;                            //ue(v)
    uint8_t     scaling_list_enabled_flag;                                      //u(1)
    uint8_t     sps_scaling_list_data_present_flag;                             //u(1)
    uint8_t     amp_enabled_flag;                                               //u(1)
    uint8_t     sample_adaptive_offset_enabled_flag;                            //u(1)
    uint8_t     pcm_enabled_flag;                                               //u(1)
    uint8_t     pcm_sample_bit_depth_luma_minus1;                               //u(4)
    uint8_t     pcm_sample_bit_depth_chroma_minus1;                             //u(4)
    uint32_t    log2_min_pcm_luma_coding_block_size_minus3;
    uint32_t    log2_max_pcm_luma_coding_block_size_minus3;                     //ue(v)
    uint32_t    log2_diff_max_min_pcm_luma_coding_block_size;                   //ue(v)
    uint8_t     pcm_loop_filter_disabled_flag;                                  //u(1)
    uint32_t    num_short_term_ref_pic_sets;                                    //ue(v)

    struct      ShortTermRefPicParamSet strp[66];
    uint8_t     long_term_ref_pics_present_flag;                                //u(1)
    uint32_t    num_long_term_ref_pics_sps;                                     //ue(v)
    uint32_t    lt_ref_pic_poc_lsb_sps[MAX_LONGTERM_REF_PIC];                   //u(v)
    uint8_t     used_by_curr_pic_lt_sps_flag[MAX_LONGTERM_REF_PIC];             //u(1)
    uint8_t     sps_temporal_mvp_enabled_flag;                                  //u(1)
    uint8_t     strong_intra_smoothing_enabled_flag;                            //u(1)
    uint8_t     vui_parameters_present_flag;                                    //u(1)
    //VuiParameters   vui_parameters;
    int         sps_extension_present_flag;                                     //u(1)
    int         sps_range_extension_flag;                                       //u(1)
    int         sps_multilayer_extension_flag;                                  //u(1)
    int         sps_3d_extension_flag;                                          //u(1)
    uint8_t     sps_extension_5bits;                                           //u(5)
    int         sps_extension_data_flag;                                        //u(1)
};
struct PicParamSet {
    uint32_t    pps_pic_parameter_set_id;                                       //ue(v)
    uint32_t    pps_seq_parameter_set_id;                                       //ue(v)
    int         dependent_slice_segments_enabled_flag;                          //u(1)
    int         output_flag_present_flag;                                       //u(1)
    uint8_t     num_extra_slice_header_bits;                                    //u(3)
    int         sign_data_hiding_enabled_flag;                                  //u(1)
    int         cabac_init_present_flag;                                        //u(1)
    uint32_t    num_ref_idx_l0_default_active_minus1;                           //ue(v)
    uint32_t    num_ref_idx_l1_default_active_minus1;                           //ue(v)
    int32_t     init_qp_minus26;                                                //se(v)
    int         constrained_intra_pred_flag;                                    //u(1)
    int         transform_skip_enabled_flag;                                    //u(1)
    int         cu_qp_delta_enabled_flag;                                       //u(1)
    uint32_t    diff_cu_qp_delta_depth;                                         //ue(v)
    uint32_t    pps_cb_qp_offset;                                               //se(v)
    uint32_t    pps_cr_qp_offset;                                               //se(v)
    int         pps_slice_chroma_qp_offsets_present_flag;                       //u(1)
    int         weighted_pred_flag;                                             //u(1)
    int         weighted_bipred_flag;                                           //u(1)
    int         transquant_bypass_enabled_flag;                                 //u(1)
    int         tiles_enabled_flag;                                             //u(1)
    int         entropy_coding_sync_enabled_flag;                               //u(1)
    uint32_t    num_tile_columns_minus1;                                        //ue(v)
    uint32_t    num_tile_rows_minus1;                                           //ue(v)
    int         uniform_spacing_flag;                                           //u(1)
    uint32_t    *column_width_minus1;                                           //ue(v)
    uint32_t    *row_height_minus1;                                             //ue(v)
    int         loop_filter_across_tiles_enabled_flag;                          //u(1)
    int         pps_loop_filter_across_slices_enabled_flag;                     //u(1)
    int         deblocking_filter_control_present_flag;                         //u(1)
    int         deblocking_filter_override_enabled_flag;                        //u(1)
    int         pps_deblocking_filter_disabled_flag;                            //u(1)
    int32_t     pps_beta_offset_div2;                                           //se(v)
    int32_t     pps_tc_offset_div2;                                             //se(v)
    int         pps_scaling_list_data_present_flag;                             //u(1)
    int         lists_modification_present_flag;                                //u(1)
    uint32_t    log2_parallel_merge_level_minus2;                               //ue(v)
    int         slice_segment_header_extension_present_flag;                    //u(1)
    int         pps_extension_present_flag;                                     //u(1)
    int         pps_range_extension_flag;                                       //u(1)
    int         pps_multilayer_extension_flag;                                  //u(1)
    int         pps_3d_extension_flag;                                          //u(1)
    uint8_t     pps_extension_5bits;                                            //u(5)
    uint8_t     pps_extension_data_flag;                                        //u(1)
    uint32_t    log2_max_transform_skip_block_size_minus2;                      //ue(v)
    uint8_t     cross_component_prediction_enabled_flag;                        //ue(1)
    uint8_t     chroma_qp_offset_list_enabled_flag;                             //ue(1)
    uint32_t    diff_cu_chroma_qp_offset_depth;                                 //ue(v)
    uint32_t    chroma_qp_offset_list_len_minus1;                               //ue(v)
    uint32_t    cb_qp_offset_list[6];                                           //se(v)
    uint32_t    cr_qp_offset_list[6];                                           //se(v)
    uint32_t    log2_sao_offset_scale_luma;                                     //ue(v)
    uint32_t    log2_sao_offset_scale_chroma;                                   //ue(v)
};
struct SliceHeader {
    int         first_slice_segment_in_pic_flag;                                //u(1)
    int         no_output_of_prior_pics_flag;                                   //u(1)
    uint32_t    slice_pic_parameter_set_id;                                     //ue(v)
    int         dependent_slice_segment_flag;                                   //u(1)
    uint32_t    picture_width_in_ctus;
    uint32_t    picture_height_in_ctus;
    uint32_t    slice_segment_address;                                          //u(v)
    int         slice_reserved_undetermined_flag[NUM_OF_EXTRA_SLICEHEADER_BITS];               //u(1)
    uint32_t    slice_type;                                                     //ue(v)
    int         pic_output_flag;                                                //u(1)
    uint8_t     colour_plane_id;                                                //u(2)
    uint32_t    pic_order_cnt_lsb;
    uint32_t    num_negative_pics;
    uint32_t    num_positive_pics;
    uint32_t    delta_poc_s0_minus1;

    struct      ShortTermRefPicParamSet strp;
    int         short_term_ref_pic_set_sps_flag;                                //u(1)
    uint32_t    short_term_ref_pic_set_idx;                                     //u(v)
    uint32_t    num_long_term_sps;                                              //ue(v)
    uint32_t    num_long_term_pics;                                             //ue(v)
    uint32_t    *lt_idx_sps;                                                    //u(v)
    uint32_t    *poc_lsb_lt;                                                    //u(v)
    int         *used_by_curr_pic_lt_flag;                                      //u(1)
    int         *delta_poc_msb_present_flag;                                    //u(1)
    uint32_t    *delta_poc_msb_cycle_lt;                                        //ue(v)
    int         slice_temporal_mvp_enabled_flag;                                //u(1)
    int         slice_sao_luma_flag;                                            //u(1)
    int         slice_sao_chroma_flag;                                          //u(1)
    int         num_ref_idx_active_override_flag;                               //u(1)
    uint32_t    num_ref_idx_l0_active_minus1;                                   //ue(v)
    uint32_t    num_ref_idx_l1_active_minus1;
    uint32_t    num_poc_total_cur;
    int         ref_pic_list_modification_flag_l0;
    int         ref_pic_list_modification_flag_l1;
    uint32_t*   list_entry_l0;
    uint32_t*   list_entry_l1;

    int         ref_pic_list_combination_flag;

    uint32_t    num_ref_idx_lc_active_minus1;
    uint32_t    ref_pic_list_modification_flag_lc;
    int         pic_from_list_0_flag;
    uint32_t    ref_idx_list_curr;
    int         mvd_l1_zero_flag;                                               //u(1)
    int         cabac_init_present_flag;
    int         pic_temporal_mvp_enable_flag;

    int         collocated_from_l0_flag;                                        //u(1)
    uint32_t    collocated_ref_idx;                                             //ue(v)
    uint32_t    five_minus_max_num_merge_cand;                                  //ue(v)
    int32_t     delta_pic_order_cnt_bottom;                                     //se(v)
    int32_t     slice_qp_delta;                                                 //se(v)
    int32_t     slice_qp_delta_cb;                                             //se(v)
    int32_t     slice_qp_delta_cr;                                             //se(v)
    int         cu_chroma_qp_offset_enabled_flag;                               //u(1)
    int         deblocking_filter_override_flag;                                //u(1)
    int         disable_deblocking_filter_flag;                          //u(1)
    int32_t     beta_offset_div2;                                         //se(v)
    int32_t     tc_offset_div2;                                           //se(v)
    int         slice_loop_filter_across_slices_enabled_flag;                   //u(1)
    uint32_t    num_entry_point_offsets;                                        //ue(v)
    uint32_t    offset_len_minus1;                                              //ue(v)
    uint32_t    *entry_point_offset;                                     //u(v)
    uint32_t    slice_segment_header_extension_length;                          //ue(v)
    uint8_t     *slice_segment_header_extension_data_byte;                      //u(8)
};

#define SURFACE_NUM 16 /* 16 surfaces for source YUV */
struct vaapi_hevc_encodec
{
    VADisplay va_dpy;
    VAProfile hevc_profile;
    VAEntrypoint entryPoint;
    VAConfigAttrib config_attrib[VAConfigAttribTypeMax];
    int config_attrib_num;
    VAConfigID config_id;
    int frame_width;
    int frame_height;
    int frame_hor_stride;
    int frame_ver_stride;
    VASurfaceID src_surface[SURFACE_NUM];
    VASurfaceID ref_surface[SURFACE_NUM];
    VABufferID  pkt_buf[SURFACE_NUM];
    VAContextID context_id;
    VAEncSequenceParameterBufferHEVC seq_param;
    VAEncPictureParameterBufferHEVC pic_param;
    VAEncSliceParameterBufferHEVC slice_param;
    struct VideoParamSet vps;
    struct SeqParamSet sps;
    struct PicParamSet pps;
    struct SliceHeader ssh;
    unsigned long long current_frame_encoding;
    unsigned long long current_pkt_idx;
    unsigned long long current_IDR_display;
    int current_frame_type;
    unsigned int numShortTerm;
};

static  int real_hevc_profile = 0;
static  int p2b = 1;
static  int lowpower = 0;
static  VAConfigAttrib attrib[VAConfigAttribTypeMax];
static  int enc_packed_header_idx;
static  struct ProfileTierParamSet protier_param;

static  VAPictureHEVC CurrentCurrPic;
static  VAPictureHEVC ReferenceFrames[16], RefPicList0_P[32], RefPicList0_B[32], RefPicList1_B[32];

static  unsigned int MaxPicOrderCntLsb = (2 << 8);

static  unsigned int num_ref_frames = 2;
static  unsigned int num_active_ref_p = 1;
static  int constraint_set_flag = 0;
static  int hevc_packedheader = 0;
static  int hevc_maxref = 16;

// static  int srcyuv_fourcc = VA_FOURCC_NV12;

static  unsigned int frame_count = 60;
static  unsigned int frame_bitrate = 0;
static  int initial_qp = 26;
static  int minimal_qp = 0;
static  int intra_period = 30;
static  int intra_idr_period = 60;
static  int ip_period = 1;
static  int rc_mode = -1;
static  int rc_default_modes[] = {
    VA_RC_VBR,
    VA_RC_CQP,
    VA_RC_VBR_CONSTRAINED,
    VA_RC_CBR,
    VA_RC_VCM,
    VA_RC_NONE,
};

static  unsigned int current_frame_num = 0;
#define current_slot (data->current_pkt_idx % SURFACE_NUM)

static  int misc_priv_type = 0;
static  int misc_priv_value = 0;

struct __bitstream {
    unsigned int *buffer;
    int bit_offset;
    int max_size_in_dword;
};
typedef struct __bitstream bitstream;

static unsigned int
va_swap32(unsigned int val)
{
    unsigned char *pval = (unsigned char *)&val;

    return ((pval[0] << 24)     |
            (pval[1] << 16)     |
            (pval[2] << 8)      |
            (pval[3] << 0));
}

static void
bitstream_start(bitstream *bs)
{
    bs->max_size_in_dword = BITSTREAM_ALLOCATE_STEPPING;
    bs->buffer = calloc(bs->max_size_in_dword * sizeof(int), 1);
    assert(bs->buffer);
    bs->bit_offset = 0;
}

static void
bitstream_end(bitstream *bs)
{
    int pos = (bs->bit_offset >> 5);
    int bit_offset = (bs->bit_offset & 0x1f);
    int bit_left = 32 - bit_offset;

    if (bit_offset) {
        bs->buffer[pos] = va_swap32((bs->buffer[pos] << bit_left));
    }
}

static void
put_ui(bitstream *bs, unsigned int val, int size_in_bits)
{
    int pos = (bs->bit_offset >> 5);
    int bit_offset = (bs->bit_offset & 0x1f);
    int bit_left = 32 - bit_offset;

    if (!size_in_bits)
        return;

    bs->bit_offset += size_in_bits;

    if (bit_left > size_in_bits) {
        bs->buffer[pos] = (bs->buffer[pos] << size_in_bits | val);
    } else {
        size_in_bits -= bit_left;
        bs->buffer[pos] = (bs->buffer[pos] << bit_left) | (val >> size_in_bits);
        bs->buffer[pos] = va_swap32(bs->buffer[pos]);

        if (pos + 1 == bs->max_size_in_dword) {
            bs->max_size_in_dword += BITSTREAM_ALLOCATE_STEPPING;
            bs->buffer = realloc(bs->buffer, bs->max_size_in_dword * sizeof(unsigned int));
            assert(bs->buffer);
        }

        bs->buffer[pos + 1] = val;
    }
}

static void
put_ue(bitstream *bs, unsigned int val)
{
    int size_in_bits = 0;
    int tmp_val = ++val;

    while (tmp_val) {
        tmp_val >>= 1;
        size_in_bits++;
    }

    put_ui(bs, 0, size_in_bits - 1); // leading zero
    put_ui(bs, val, size_in_bits);
}

static void
put_se(bitstream *bs, int val)
{
    unsigned int new_val;

    if (val <= 0)
        new_val = -2 * val;
    else
        new_val = 2 * val - 1;

    put_ue(bs, new_val);
}

static void
byte_aligning(bitstream *bs, int bit)
{
    int bit_offset = (bs->bit_offset & 0x7);
    int bit_left = 8 - bit_offset;
    int new_val;

    if (!bit_offset)
        return;

    assert(bit == 0 || bit == 1);

    if (bit)
        new_val = (1 << bit_left) - 1;
    else
        new_val = 0;

    put_ui(bs, new_val, bit_left);
}

static void
rbsp_trailing_bits(bitstream *bs)
{
    put_ui(bs, 1, 1);
    byte_aligning(bs, 0);
}

static void nal_start_code_prefix(bitstream *bs, int nal_unit_type)
{
    if (nal_unit_type == NALU_VPS ||
        nal_unit_type == NALU_SPS ||
        nal_unit_type == NALU_PPS ||
        nal_unit_type == NALU_AUD)
        put_ui(bs, 0x00000001, 32);
    else
        put_ui(bs, 0x000001, 24);
}

static void nal_header(bitstream *bs, int nal_unit_type)
{
    put_ui(bs, 0, 1);                /* forbidden_zero_bit: 0 */
    put_ui(bs, nal_unit_type, 6);
    put_ui(bs, 0, 6);
    put_ui(bs, 1, 3);
}

static int calc_poc(struct vaapi_hevc_encodec *data, int pic_order_cnt_lsb)
{
    static int picOrderCntMsb_ref = 0, pic_order_cnt_lsb_ref = 0;
    int prevPicOrderCntMsb, prevPicOrderCntLsb;
    int picOrderCntMsb, picOrderCnt;

    if (data->current_frame_type == FRAME_IDR)
        prevPicOrderCntMsb = prevPicOrderCntLsb = 0;
    else {
        prevPicOrderCntMsb = picOrderCntMsb_ref;
        prevPicOrderCntLsb = pic_order_cnt_lsb_ref;
    }

    if ((pic_order_cnt_lsb < prevPicOrderCntLsb) &&
        ((prevPicOrderCntLsb - pic_order_cnt_lsb) >= (int)(MaxPicOrderCntLsb / 2)))
        picOrderCntMsb = prevPicOrderCntMsb + MaxPicOrderCntLsb;
    else if ((pic_order_cnt_lsb > prevPicOrderCntLsb) &&
             ((pic_order_cnt_lsb - prevPicOrderCntLsb) > (int)(MaxPicOrderCntLsb / 2)))
        picOrderCntMsb = prevPicOrderCntMsb - MaxPicOrderCntLsb;
    else
        picOrderCntMsb = prevPicOrderCntMsb;

    picOrderCnt = picOrderCntMsb + pic_order_cnt_lsb;

    if (data->current_frame_type != FRAME_B) {
        picOrderCntMsb_ref = picOrderCntMsb;
        pic_order_cnt_lsb_ref = pic_order_cnt_lsb;
    }

    return picOrderCnt;
}

static void fill_profile_tier_level(
    uint8_t vps_max_layers_minus1,
    struct ProfileTierParamSet *ptps,
    uint8_t profilePresentFlag)
{
    if (!profilePresentFlag)
        return;

    memset(ptps, 0, sizeof(*ptps));

    ptps->general_profile_space = 0;
    ptps->general_tier_flag = 0;
    ptps->general_profile_idc = real_hevc_profile;
    memset(ptps->general_profile_compatibility_flag, 0, 32 * sizeof(int));
    ptps->general_profile_compatibility_flag[ptps->general_profile_idc] = 1;
    ptps->general_progressive_source_flag = 1;
    ptps->general_interlaced_source_flag = 0;
    ptps->general_non_packed_constraint_flag = 0;
    ptps->general_frame_only_constraint_flag = 1;

    ptps->general_level_idc = 30;
    ptps->general_level_idc = ptps->general_level_idc * 4;

}
static void fill_vps_header(struct VideoParamSet *vps)
{
    int i = 0;
    memset(vps, 0, sizeof(*vps));

    vps->vps_video_parameter_set_id = 0;
    vps->vps_base_layer_internal_flag = 1;
    vps->vps_base_layer_available_flag = 1;
    vps->vps_max_layers_minus1 = 0;
    vps->vps_max_sub_layers_minus1 = 0; // max temporal layer minus 1
    vps->vps_temporal_id_nesting_flag = 1;
    vps->vps_reserved_0xffff_16bits = 0xFFFF;
    // hevc::ProfileTierParamSet ptps;
    memset(&vps->ptps, 0, sizeof(vps->ptps));
    fill_profile_tier_level(vps->vps_max_layers_minus1, &protier_param, 1);
    vps->vps_sub_layer_ordering_info_present_flag = 0;
    for (i = 0; i < MAX_TEMPORAL_SUBLAYERS; i++) {
        vps->vps_max_dec_pic_buffering_minus1[i] = intra_period == 1 ? 1 : 6;
        vps->vps_max_num_reorder_pics[i] = ip_period != 0 ? ip_period - 1 : 0;
        vps->vps_max_latency_increase_plus1[i] = 0;
    }
    vps->vps_max_layer_id = 0;
    vps->vps_num_layer_sets_minus1 = 0;
    vps->vps_sub_layer_ordering_info_present_flag = 0;
    vps->vps_max_nuh_reserved_zero_layer_id = 0;
    vps->vps_max_op_sets = 1;
    vps->vps_timing_info_present_flag = 0;
    vps->vps_extension_flag = 0;
}

static void fill_short_term_ref_pic_header(
    struct ShortTermRefPicParamSet  *strp,
    uint8_t strp_index)
{
    uint32_t i = 0;
    // inter_ref_pic_set_prediction_flag is always 0 now
    strp->inter_ref_pic_set_prediction_flag = 0;
    /* don't need to set below parameters since inter_ref_pic_set_prediction_flag equal to 0
    strp->delta_idx_minus1 should be set to 0 since strp_index != num_short_term_ref_pic_sets in sps
    strp->delta_rps_sign;
    strp->abs_delta_rps_minus1;
    strp->used_by_curr_pic_flag[j];
    strp->use_delta_flag[j];
    */
    strp->num_negative_pics = num_active_ref_p;
    int num_positive_pics = ip_period > 1 ? 1 : 0;
    strp->num_positive_pics = strp_index == 0 ? 0 : num_positive_pics;

    if (strp_index == 0) {
        for (i = 0; i < strp->num_negative_pics; i++) {
            strp->delta_poc_s0_minus1[i] = ip_period - 1;
            strp->used_by_curr_pic_s0_flag[i] = 1;
        }
    } else {
        for (i = 0; i < strp->num_negative_pics; i++) {
            strp->delta_poc_s0_minus1[i] = (i == 0) ?
                                           (strp_index - 1) : (ip_period - 1);
            strp->used_by_curr_pic_s0_flag[i] = 1;
        }
        for (i = 0; i < strp->num_positive_pics; i++) {
            strp->delta_poc_s1_minus1[i] = ip_period - 1 - strp_index;
            strp->used_by_curr_pic_s1_flag[i] = 1;
        }

    }
}

void fill_sps_header(struct vaapi_hevc_encodec *data, int id)
{
    int i = 0;
    memset(&data->sps, 0, sizeof(struct  SeqParamSet));

    data->sps.sps_video_parameter_set_id = 0;
    data->sps.sps_max_sub_layers_minus1 = 0;
    data->sps.sps_temporal_id_nesting_flag = 1;
    fill_profile_tier_level(data->sps.sps_max_sub_layers_minus1, &data->sps.ptps, 1);
    data->sps.sps_seq_parameter_set_id = id;
    data->sps.chroma_format_idc = 1;
    if (data->sps.chroma_format_idc == 3) {
        data->sps.separate_colour_plane_flag = 0;
    }
    data->frame_hor_stride = ALIGN16(data->frame_width);
    data->frame_ver_stride = ALIGN16(data->frame_height);
    data->sps.pic_width_in_luma_samples = data->frame_hor_stride;
    data->sps.pic_height_in_luma_samples = data->frame_ver_stride;
    if (data->frame_hor_stride != data->frame_width ||
        data->frame_ver_stride != data->frame_height) {
        data->sps.conformance_window_flag = 1;
        data->sps.conf_win_left_offset = 0;
        data->sps.conf_win_top_offset = 0;
        switch (data->sps.chroma_format_idc) {
        case 0:
        case 3:  // 4:4:4 format
            data->sps.conf_win_right_offset = (data->frame_hor_stride - data->frame_width);
            data->sps.conf_win_bottom_offset = (data->frame_ver_stride - data->frame_height);
            break;

        case 2:  // 4:2:2 format
            data->sps.conf_win_right_offset = (data->frame_hor_stride - data->frame_width) >> 1;
            data->sps.conf_win_bottom_offset = (data->frame_ver_stride - data->frame_height);
            break;

        case 1:
        default: // 4:2:0 format
            data->sps.conf_win_right_offset = (data->frame_hor_stride - data->frame_width) >> 1;
            data->sps.conf_win_bottom_offset = (data->frame_ver_stride - data->frame_height) >> 1;
            break;
        }
    } else {
        data->sps.conformance_window_flag = 0;
    }

    data->sps.bit_depth_luma_minus8 = 0;
    data->sps.bit_depth_chroma_minus8 = 0;
    data->sps.log2_max_pic_order_cnt_lsb_minus4 = MAX((ceil(log(ip_period - 1 + 4) / log(2.0)) + 3), 4) - 4;
    data->sps.sps_sub_layer_ordering_info_present_flag = 0;
    for (i = 0; i < MAX_TEMPORAL_SUBLAYERS; i++) {
        data->sps.sps_max_dec_pic_buffering_minus1[i] = intra_period == 1 ? 1 : 6;
        data->sps.sps_max_num_reorder_pics[i] = ip_period != 0 ? ip_period - 1 : 0;
        data->sps.sps_max_latency_increase_plus1[i] = 0;
    }
    data->sps.log2_min_luma_coding_block_size_minus3 = 0;
    int log2_max_luma_coding_block_size = log2(LCU_SIZE);
    int log2_min_luma_coding_block_size = data->sps.log2_min_luma_coding_block_size_minus3 + 3;
    data->sps.log2_diff_max_min_luma_coding_block_size = log2_max_luma_coding_block_size -
            log2_min_luma_coding_block_size;
    data->sps.log2_min_luma_transform_block_size_minus2 = 0;
    data->sps.log2_diff_max_min_luma_transform_block_size = 3;
    data->sps.max_transform_hierarchy_depth_inter = 2;
    data->sps.max_transform_hierarchy_depth_intra = 2;
    data->sps.scaling_list_enabled_flag = 0;
    //data->sps.sps_scaling_list_data_present_flag; // ignore since scaling_list_enabled_flag equal to 0
    data->sps.amp_enabled_flag = 1;
    data->sps.sample_adaptive_offset_enabled_flag = 1;
    data->sps.pcm_enabled_flag = 0;
    /* ignore below parameters seting since pcm_enabled_flag equal to 0
    pcm_sample_bit_depth_luma_minus1;
    pcm_sample_bit_depth_chroma_minus1;
    log2_min_pcm_luma_coding_block_size_minus3;
    log2_diff_max_min_pcm_luma_coding_block_size;
    pcm_loop_filter_disabled_flag;
    */
    data->sps.num_short_term_ref_pic_sets = ip_period;

    memset(&data->sps.strp[0], 0, sizeof(data->sps.strp));
    for (i = 0; i < MIN(data->sps.num_short_term_ref_pic_sets, 64); i++)
        fill_short_term_ref_pic_header(&data->sps.strp[i], i);
    data->sps.long_term_ref_pics_present_flag = 0;
    /* ignore below parameters seting since long_term_ref_pics_present_flag equal to 0
    num_long_term_ref_pics_sps;
    lt_ref_pic_poc_lsb_sps[kMaxLongTermRefPic];
    used_by_curr_pic_lt_sps_flag[kMaxLongTermRefPic];
    */
    data->sps.sps_temporal_mvp_enabled_flag = 1;
    data->sps.strong_intra_smoothing_enabled_flag = 0;

    data->sps.vui_parameters_present_flag = 0;
    data->sps.sps_extension_present_flag = 0;
    /* ignore below parameters seting since sps_extension_present_flag equal to 0
    data->sps.sps_range_extension_flag
    data->sps.sps_multilayer_extension_flag
    data->sps.sps_3d_extension_flag
    data->sps.sps_extension_5bits
    data->sps.sps_extension_data_flag
    */
}

static void fill_pps_header(
    struct PicParamSet *pps,
    uint32_t pps_id,
    uint32_t sps_id)
{
    memset(pps, 0, sizeof(struct PicParamSet));

    pps->pps_pic_parameter_set_id = pps_id;
    pps->pps_seq_parameter_set_id = sps_id;
    pps->dependent_slice_segments_enabled_flag = 0;
    pps->output_flag_present_flag = 0;
    pps->num_extra_slice_header_bits = 0;
    pps->sign_data_hiding_enabled_flag = 0;
    pps->cabac_init_present_flag = 1;

    pps->num_ref_idx_l0_default_active_minus1 = 0;
    pps->num_ref_idx_l1_default_active_minus1 = 0;

    pps->init_qp_minus26 = initial_qp - 26;
    pps->constrained_intra_pred_flag = 0;
    pps->transform_skip_enabled_flag = 0;
    pps->cu_qp_delta_enabled_flag = 1;
    if (pps->cu_qp_delta_enabled_flag)
        pps->diff_cu_qp_delta_depth = 2;
    pps->pps_cb_qp_offset = 0;
    pps->pps_cr_qp_offset = 0;
    pps->pps_slice_chroma_qp_offsets_present_flag = 0;
    pps->weighted_pred_flag = 0;
    pps->weighted_bipred_flag = 0;
    pps->transquant_bypass_enabled_flag = 0;
    pps->entropy_coding_sync_enabled_flag = 0;
    pps->tiles_enabled_flag = 0;

    pps->pps_loop_filter_across_slices_enabled_flag = 0;
    pps->deblocking_filter_control_present_flag = 1;
    pps->deblocking_filter_override_enabled_flag = 0,
         pps->pps_deblocking_filter_disabled_flag = 0,
              pps->pps_beta_offset_div2 = 2,
                   pps->pps_tc_offset_div2 = 0,
                        pps->pps_scaling_list_data_present_flag = 0;
    pps->lists_modification_present_flag = 0;
    pps->log2_parallel_merge_level_minus2 = 0;
    pps->slice_segment_header_extension_present_flag = 0;
    pps->pps_extension_present_flag = 0;
    pps->pps_range_extension_flag = 0;

}
static void fill_slice_header(struct vaapi_hevc_encodec *data)
{
    memset(&data->ssh, 0, sizeof(struct SliceHeader));
    data->ssh.pic_output_flag = 1;
    data->ssh.colour_plane_id = 0;
    data->ssh.no_output_of_prior_pics_flag = 0;
    data->ssh.pic_order_cnt_lsb = calc_poc(data, (data->current_pkt_idx - data->current_IDR_display) % MaxPicOrderCntLsb);

    //slice_segment_address (u(v))
    data->ssh.picture_height_in_ctus = (data->frame_height + LCU_SIZE - 1) / LCU_SIZE;
    data->ssh.picture_width_in_ctus = (data->frame_width + LCU_SIZE - 1) / LCU_SIZE;
    data->ssh.slice_segment_address = 0;
    data->ssh.first_slice_segment_in_pic_flag = ((data->ssh.slice_segment_address == 0) ? 1 : 0);
    data->ssh.slice_type = data->current_frame_type == FRAME_P ? (p2b ? SLICE_B : SLICE_P) :
                        data->current_frame_type == FRAME_B ? SLICE_B : SLICE_I;

    data->ssh.dependent_slice_segment_flag = 0;
    data->ssh.short_term_ref_pic_set_sps_flag = 1;
    data->ssh.num_ref_idx_active_override_flag = 0;
    data->ssh.short_term_ref_pic_set_idx = data->ssh.pic_order_cnt_lsb % ip_period;
    data->ssh.strp.num_negative_pics = data->numShortTerm;
    data->ssh.strp.num_positive_pics = 0;
    data->ssh.slice_sao_luma_flag = 0;
    data->ssh.slice_sao_chroma_flag = 0;
    data->ssh.slice_temporal_mvp_enabled_flag = 1;

    data->ssh.num_ref_idx_l0_active_minus1 = data->pps.num_ref_idx_l0_default_active_minus1;
    data->ssh.num_ref_idx_l1_active_minus1 = data->pps.num_ref_idx_l1_default_active_minus1;

    data->ssh.num_poc_total_cur = 0;
    // for I slice
    if (data->current_frame_type == FRAME_I || data->current_frame_type == FRAME_IDR) {
        data->ssh.ref_pic_list_modification_flag_l0 = 0;
        data->ssh.list_entry_l0 = 0;
        data->ssh.ref_pic_list_modification_flag_l1 = 0;
        data->ssh.list_entry_l1 = 0;
    } else {
        data->ssh.ref_pic_list_modification_flag_l0 = 1;
        data->ssh.num_poc_total_cur = 2;
    }

    data->ssh.ref_pic_list_combination_flag = 0;
    data->ssh.num_ref_idx_lc_active_minus1 = 0;
    data->ssh.ref_pic_list_modification_flag_lc = 0;
    data->ssh.pic_from_list_0_flag = 0;
    data->ssh.ref_idx_list_curr = 0;
    data->ssh.mvd_l1_zero_flag = 0;
    data->ssh.cabac_init_present_flag = 0;

    data->ssh.slice_qp_delta = 0;
    data->ssh.slice_qp_delta_cb = data->pps.pps_cb_qp_offset;
    data->ssh.slice_qp_delta_cr = data->pps.pps_cr_qp_offset;

    data->ssh.deblocking_filter_override_flag = 0;
    data->ssh.disable_deblocking_filter_flag = 0;
    data->ssh.tc_offset_div2 = data->pps.pps_tc_offset_div2;
    data->ssh.beta_offset_div2 = data->pps.pps_beta_offset_div2;

    data->ssh.collocated_from_l0_flag = 1;
    data->ssh.collocated_ref_idx = data->pps.num_ref_idx_l0_default_active_minus1;

    data->ssh.five_minus_max_num_merge_cand = 0;

    data->ssh.slice_loop_filter_across_slices_enabled_flag = 0;
    data->ssh.num_entry_point_offsets = 0;
    data->ssh.offset_len_minus1 = 0;
}

static void protier_rbsp(bitstream *bs)
{
    uint32_t i = 0;
    put_ui(bs, protier_param.general_profile_space, 2);
    put_ui(bs, protier_param.general_tier_flag, 1);
    put_ui(bs, protier_param.general_profile_idc, 5);

    for (i = 0; i < 32; i++)
        put_ui(bs, protier_param.general_profile_compatibility_flag[i], 1);

    put_ui(bs, protier_param.general_progressive_source_flag, 1);
    put_ui(bs, protier_param.general_interlaced_source_flag, 1);
    put_ui(bs, protier_param.general_non_packed_constraint_flag, 1);
    put_ui(bs, protier_param.general_frame_only_constraint_flag, 1);
    put_ui(bs, 0, 16);
    put_ui(bs, 0, 16);
    put_ui(bs, 0, 12);
    put_ui(bs, protier_param.general_level_idc, 8);
}
void pack_short_term_ref_pic_setp(
    bitstream *bs,
    struct ShortTermRefPicParamSet* strp,
    int first_strp)
{
    uint32_t i = 0;
    if (!first_strp)
        put_ui(bs, strp->inter_ref_pic_set_prediction_flag, 1);

    // inter_ref_pic_set_prediction_flag is always 0 now
    put_ue(bs, strp->num_negative_pics);
    put_ue(bs, strp->num_positive_pics);

    for (i = 0; i < strp->num_negative_pics; i++) {
        put_ue(bs, strp->delta_poc_s0_minus1[i]);
        put_ui(bs, strp->used_by_curr_pic_s0_flag[i], 1);
    }
    for (i = 0; i < strp->num_positive_pics; i++) {
        put_ue(bs, strp->delta_poc_s1_minus1[i]);
        put_ui(bs, strp->used_by_curr_pic_s1_flag[i], 1);
    }
}
static void vps_rbsp(struct vaapi_hevc_encodec *data, bitstream *bs)
{
    uint32_t i = 0;
    put_ui(bs, data->vps.vps_video_parameter_set_id, 4);
    put_ui(bs, 3, 2);  //vps_reserved_three_2bits
    put_ui(bs, 0, 6);  //vps_reserved_zero_6bits

    put_ui(bs, data->vps.vps_max_sub_layers_minus1, 3);
    put_ui(bs, data->vps.vps_temporal_id_nesting_flag, 1);
    put_ui(bs, 0xFFFF, 16); //vps_reserved_0xffff_16bits
    protier_rbsp(bs);

    put_ui(bs, data->vps.vps_sub_layer_ordering_info_present_flag, 1);

    for (i = (data->vps.vps_sub_layer_ordering_info_present_flag ? 0 : data->vps.vps_max_sub_layers_minus1); i <= data->vps.vps_max_sub_layers_minus1; i++) {
        // NOTE: In teddi and mv_encoder, the setting is max_dec_pic_buffering.
        // here just follow the spec 7.3.2.1
        put_ue(bs, data->vps.vps_max_dec_pic_buffering_minus1[i]);
        put_ue(bs, data->vps.vps_max_num_reorder_pics[i]);
        put_ue(bs, data->vps.vps_max_latency_increase_plus1[i]);
    }

    put_ui(bs, data->vps.vps_max_nuh_reserved_zero_layer_id, 6);
    put_ue(bs, data->vps.vps_num_op_sets_minus1);

    put_ui(bs, data->vps.vps_timing_info_present_flag, 1);

    if (data->vps.vps_timing_info_present_flag) {
        put_ue(bs, data->vps.vps_num_units_in_tick);
        put_ue(bs, data->vps.vps_time_scale);
        put_ue(bs, data->vps.vps_poc_proportional_to_timing_flag);
        if (data->vps.vps_poc_proportional_to_timing_flag) {
            put_ue(bs, data->vps.vps_num_ticks_poc_diff_one_minus1);
        }
        put_ue(bs, data->vps.vps_num_hrd_parameters);
        for (i = 0; i < data->vps.vps_num_hrd_parameters; i++) {
            put_ue(bs, data->vps.hrd_layer_set_idx[i]);
            if (i > 0) {
                put_ui(bs, data->vps.cprms_present_flag[i], 1);
            }
        }
    }

    // no extension flag
    put_ui(bs, 0, 1);
}

static void sps_rbsp(struct vaapi_hevc_encodec *data, bitstream *bs)
{
    uint32_t  i = 0;
    put_ui(bs, data->sps.sps_video_parameter_set_id, 4);
    put_ui(bs, data->sps.sps_max_sub_layers_minus1, 3);
    put_ui(bs, data->sps.sps_temporal_id_nesting_flag, 1);

    protier_rbsp(bs);

    put_ue(bs, data->sps.sps_seq_parameter_set_id);
    put_ue(bs, data->sps.chroma_format_idc);

    if (data->sps.chroma_format_idc == 3) {
        put_ui(bs, data->sps.separate_colour_plane_flag, 1);

    }
    put_ue(bs, data->sps.pic_width_in_luma_samples);
    put_ue(bs, data->sps.pic_height_in_luma_samples);

    put_ui(bs, data->sps.conformance_window_flag, 1);

    if (data->sps.conformance_window_flag) {
        put_ue(bs, data->sps.conf_win_left_offset);
        put_ue(bs, data->sps.conf_win_right_offset);
        put_ue(bs, data->sps.conf_win_top_offset);
        put_ue(bs, data->sps.conf_win_bottom_offset);
    }
    put_ue(bs, data->sps.bit_depth_luma_minus8);
    put_ue(bs, data->sps.bit_depth_chroma_minus8);
    put_ue(bs, data->sps.log2_max_pic_order_cnt_lsb_minus4);
    put_ui(bs, data->sps.sps_sub_layer_ordering_info_present_flag, 1);

    for (i = (data->sps.sps_sub_layer_ordering_info_present_flag ? 0 : data->sps.sps_max_sub_layers_minus1); i <= data->sps.sps_max_sub_layers_minus1; i++) {
        // NOTE: In teddi and mv_encoder, the setting is max_dec_pic_buffering.
        // here just follow the spec 7.3.2.2
        put_ue(bs, data->sps.sps_max_dec_pic_buffering_minus1[i]);
        put_ue(bs, data->sps.sps_max_num_reorder_pics[i]);
        put_ue(bs, data->sps.sps_max_latency_increase_plus1[i]);
    }

    put_ue(bs, data->sps.log2_min_luma_coding_block_size_minus3);
    put_ue(bs, data->sps.log2_diff_max_min_luma_coding_block_size);
    put_ue(bs, data->sps.log2_min_luma_transform_block_size_minus2);
    put_ue(bs, data->sps.log2_diff_max_min_luma_transform_block_size);
    put_ue(bs, data->sps.max_transform_hierarchy_depth_inter);
    put_ue(bs, data->sps.max_transform_hierarchy_depth_intra);

    // scaling_list_enabled_flag is set as 0 in fill_sps_header() for now
    put_ui(bs, data->sps.scaling_list_enabled_flag, 1);
    if (data->sps.scaling_list_enabled_flag) {
        put_ui(bs, data->sps.sps_scaling_list_data_present_flag, 1);
        if (data->sps.sps_scaling_list_data_present_flag) {
            //scaling_list_data();
        }
    }

    put_ui(bs, data->sps.amp_enabled_flag, 1);
    put_ui(bs, data->sps.sample_adaptive_offset_enabled_flag, 1);

    // pcm_enabled_flag is set as 0 in fill_sps_header() for now
    put_ui(bs, data->sps.pcm_enabled_flag, 1);
    if (data->sps.pcm_enabled_flag) {
        put_ui(bs, data->sps.pcm_sample_bit_depth_luma_minus1, 4);
        put_ui(bs, data->sps.pcm_sample_bit_depth_chroma_minus1, 4);
        put_ue(bs, data->sps.log2_min_pcm_luma_coding_block_size_minus3);
        put_ue(bs, data->sps.log2_diff_max_min_pcm_luma_coding_block_size);
        put_ui(bs, data->sps.pcm_loop_filter_disabled_flag, 1);
    }

    put_ue(bs, data->sps.num_short_term_ref_pic_sets);
    for (i = 0; i < data->sps.num_short_term_ref_pic_sets; i++) {
        pack_short_term_ref_pic_setp(bs, &data->sps.strp[i], i == 0);
    }

    // long_term_ref_pics_present_flag is set as 0 in fill_sps_header() for now
    put_ui(bs, data->sps.long_term_ref_pics_present_flag, 1);
    if (data->sps.long_term_ref_pics_present_flag) {
        put_ue(bs, data->sps.num_long_term_ref_pics_sps);
        for (i = 0; i < data->sps.num_long_term_ref_pics_sps; i++) {
            put_ue(bs, data->sps.lt_ref_pic_poc_lsb_sps[i]);
            put_ui(bs, data->sps.used_by_curr_pic_lt_sps_flag[i], 1);
        }
    }

    put_ui(bs, data->sps.sps_temporal_mvp_enabled_flag, 1);
    put_ui(bs, data->sps.strong_intra_smoothing_enabled_flag, 1);

    // vui_parameters_present_flag is set as 0 in fill_sps_header() for now
    put_ui(bs, data->sps.vui_parameters_present_flag, 1);

    put_ui(bs, data->sps.sps_extension_present_flag, 1);
}

static void pps_rbsp(struct vaapi_hevc_encodec *data, bitstream *bs)
{
    uint32_t  i = 0;
    put_ue(bs, data->pps.pps_pic_parameter_set_id);
    put_ue(bs, data->pps.pps_seq_parameter_set_id);
    put_ui(bs, data->pps.dependent_slice_segments_enabled_flag, 1);
    put_ui(bs, data->pps.output_flag_present_flag, 1);
    put_ui(bs, data->pps.num_extra_slice_header_bits, 3);
    put_ui(bs, data->pps.sign_data_hiding_enabled_flag, 1);
    put_ui(bs, data->pps.cabac_init_present_flag, 1);

    put_ue(bs, data->pps.num_ref_idx_l0_default_active_minus1);
    put_ue(bs, data->pps.num_ref_idx_l1_default_active_minus1);
    put_se(bs, data->pps.init_qp_minus26);

    put_ui(bs, data->pps.constrained_intra_pred_flag, 1);
    put_ui(bs, data->pps.transform_skip_enabled_flag, 1);

    put_ui(bs, data->pps.cu_qp_delta_enabled_flag, 1);
    if (data->pps.cu_qp_delta_enabled_flag) {
        put_ue(bs, data->pps.diff_cu_qp_delta_depth);
    }

    put_se(bs, data->pps.pps_cb_qp_offset);
    put_se(bs, data->pps.pps_cr_qp_offset);

    put_ui(bs, data->pps.pps_slice_chroma_qp_offsets_present_flag, 1);
    put_ui(bs, data->pps.weighted_pred_flag, 1);
    put_ui(bs, data->pps.weighted_bipred_flag, 1);
    put_ui(bs, data->pps.transquant_bypass_enabled_flag, 1);
    put_ui(bs, data->pps.tiles_enabled_flag, 1);
    put_ui(bs, data->pps.entropy_coding_sync_enabled_flag, 1);

    if (data->pps.tiles_enabled_flag) {
        put_ue(bs, data->pps.num_tile_columns_minus1);
        put_ue(bs, data->pps.num_tile_rows_minus1);
        put_ui(bs, data->pps.uniform_spacing_flag, 1);
        if (!data->pps.uniform_spacing_flag) {
            for (i = 0; i < data->pps.num_tile_columns_minus1; i++) {
                put_ue(bs, data->pps.column_width_minus1[i]);
            }

            for (i = 0; i < data->pps.num_tile_rows_minus1; i++) {
                put_ue(bs, data->pps.row_height_minus1[i]);
            }

        }
        put_ui(bs, data->pps.loop_filter_across_tiles_enabled_flag, 1);
    }

    put_ui(bs, data->pps.pps_loop_filter_across_slices_enabled_flag, 1);
    put_ui(bs, data->pps.deblocking_filter_control_present_flag, 1);
    if (data->pps.deblocking_filter_control_present_flag) {
        put_ui(bs, data->pps.deblocking_filter_override_enabled_flag, 1);
        put_ui(bs, data->pps.pps_deblocking_filter_disabled_flag, 1);
        if (!data->pps.pps_deblocking_filter_disabled_flag) {
            put_se(bs, data->pps.pps_beta_offset_div2);
            put_se(bs, data->pps.pps_tc_offset_div2);
        }
    }

    // pps_scaling_list_data_present_flag is set as 0 in fill_pps_header() for now
    put_ui(bs, data->pps.pps_scaling_list_data_present_flag, 1);
    if (data->pps.pps_scaling_list_data_present_flag) {
        //scaling_list_data();
    }

    put_ui(bs, data->pps.lists_modification_present_flag, 1);
    put_ue(bs, data->pps.log2_parallel_merge_level_minus2);
    put_ui(bs, data->pps.slice_segment_header_extension_present_flag, 1);

    put_ui(bs, data->pps.pps_extension_present_flag, 1);
    if (data->pps.pps_extension_present_flag) {
        put_ui(bs, data->pps.pps_range_extension_flag, 1);
        put_ui(bs, data->pps.pps_multilayer_extension_flag, 1);
        put_ui(bs, data->pps.pps_3d_extension_flag, 1);
        put_ui(bs, data->pps.pps_extension_5bits, 1);

    }

    if (data->pps.pps_range_extension_flag) {
        if (data->pps.transform_skip_enabled_flag)
            put_ue(bs, data->pps.log2_max_transform_skip_block_size_minus2);
        put_ui(bs, data->pps.cross_component_prediction_enabled_flag, 1);
        put_ui(bs, data->pps.chroma_qp_offset_list_enabled_flag, 1);

        if (data->pps.chroma_qp_offset_list_enabled_flag) {
            put_ue(bs, data->pps.diff_cu_chroma_qp_offset_depth);
            put_ue(bs, data->pps.chroma_qp_offset_list_len_minus1);
            for (i = 0; i <= data->pps.chroma_qp_offset_list_len_minus1; i++) {
                put_ue(bs, data->pps.cb_qp_offset_list[i]);
                put_ue(bs, data->pps.cr_qp_offset_list[i]);
            }
        }

        put_ue(bs, data->pps.log2_sao_offset_scale_luma);
        put_ue(bs, data->pps.log2_sao_offset_scale_chroma);
    }

}
static void sliceHeader_rbsp(
    bitstream *bs,
    struct SliceHeader *slice_header,
    struct SeqParamSet *sps,
    struct PicParamSet *pps,
    int isidr)
{
    uint8_t nal_unit_type = NALU_TRAIL_R;
    int gop_ref_distance = ip_period;
    int incomplete_mini_gop = 0;
    int p_slice_flag = 1;
    int i = 0;

    put_ui(bs, slice_header->first_slice_segment_in_pic_flag, 1);
    if (slice_header->pic_order_cnt_lsb == 0)
        nal_unit_type = NALU_IDR_W_DLP;

    if (nal_unit_type >= 16 && nal_unit_type <= 23)
        put_ui(bs, slice_header->no_output_of_prior_pics_flag, 1);

    put_ue(bs, slice_header->slice_pic_parameter_set_id);

    if (!slice_header->first_slice_segment_in_pic_flag) {
        if (slice_header->dependent_slice_segment_flag) {
            put_ui(bs, slice_header->dependent_slice_segment_flag, 1);
        }

        put_ui(bs, slice_header->slice_segment_address,
               (uint8_t)(ceil(log(slice_header->picture_height_in_ctus * slice_header->picture_width_in_ctus) / log(2.0))));
    }
    if (!slice_header->dependent_slice_segment_flag) {
        for (i = 0; i < pps->num_extra_slice_header_bits; i++) {
            put_ui(bs, slice_header->slice_reserved_undetermined_flag[i], 1);
        }
        put_ue(bs, slice_header->slice_type);
        if (pps->output_flag_present_flag) {
            put_ui(bs, slice_header->pic_output_flag, 1);
        }
        if (sps->separate_colour_plane_flag == 1) {
            put_ui(bs, slice_header->colour_plane_id, 2);
        }

        if (!(nal_unit_type == NALU_IDR_W_DLP || nal_unit_type == NALU_IDR_N_LP)) {
            put_ui(bs, slice_header->pic_order_cnt_lsb, (sps->log2_max_pic_order_cnt_lsb_minus4 + 4));
            put_ui(bs, slice_header->short_term_ref_pic_set_sps_flag, 1);

            if (!slice_header->short_term_ref_pic_set_sps_flag) {
                // refer to Teddi
                if (sps->num_short_term_ref_pic_sets > 0)
                    put_ui(bs, 0, 1); // inter_ref_pic_set_prediction_flag, always 0 for now

                put_ue(bs, slice_header->strp.num_negative_pics);
                put_ue(bs, slice_header->strp.num_positive_pics);

                // below chunks of codes (majorly two big 'for' blocks) are refering both
                // Teddi and mv_encoder, they look kind of ugly, however, keep them as these
                // since it will be pretty easy to update if change/update in Teddi side.
                // According to Teddi, these are CModel Implementation.
                int prev = 0;
                int frame_cnt_in_gop = slice_header->pic_order_cnt_lsb / 2;
                // this is the first big 'for' block
                for (i = 0; i < slice_header->strp.num_negative_pics; i++) {
                    // Low Delay B case
                    if (1 == gop_ref_distance) {
                        put_ue(bs, 0 /*delta_poc_s0_minus1*/);
                    } else {
                        if (incomplete_mini_gop) {
                            if (frame_cnt_in_gop % gop_ref_distance > i) {
                                put_ue(bs, 0 /*delta_poc_s0_minus1*/);
                            } else {
                                int DeltaPoc = -(int)(gop_ref_distance);
                                put_ue(bs, prev - DeltaPoc - 1 /*delta_poc_s0_minus1*/);
                            }
                        } else {
                            // For Non-BPyramid GOP i.e B0 type
                            if (num_active_ref_p > 1) {
                                // MultiRef Case
                                if (p_slice_flag) {
                                    // DeltaPOC Equals NumB
                                    int DeltaPoc = -(int)(gop_ref_distance);
                                    put_ue(bs, prev - DeltaPoc - 1 /*delta_poc_s0_minus1*/);
                                } else {
                                    // for normal B
                                    if (frame_cnt_in_gop < gop_ref_distance) {
                                        if (0 == i) {
                                            int DeltaPoc = -(int)(frame_cnt_in_gop);
                                            put_ue(bs, prev - DeltaPoc - 1 /*delta_poc_s0_minus1*/);
                                        }
                                    } else if (frame_cnt_in_gop > gop_ref_distance) {
                                        if (0 == i) {
                                            //Need % to wraparound the delta poc, to avoid corruption caused on POC=5 with GOP (29,2) and 4 refs
                                            int DeltaPoc = -(int)((frame_cnt_in_gop - gop_ref_distance) % gop_ref_distance);
                                            put_ue(bs, prev - DeltaPoc - 1 /*delta_poc_s0_minus1*/);
                                        } else if (1 <= i) {
                                            int DeltaPoc = -(int)(gop_ref_distance);
                                            put_ue(bs, prev - DeltaPoc - 1 /*delta_poc_s0_minus1*/);
                                        }
                                    }
                                }
                            } else {
                                //  the big 'if' wraps here is -
                                //     if (!slice_header->short_term_ref_pic_set_sps_flag)
                                // From the Teddi logic, the short_term_ref_pic_set_sps_flag only can be '0'
                                // either for B-Prymid or first several frames in a GOP in multi-ref cases
                                // when there are not enough backward refs.
                                // So though there are really some codes under this 'else'in Teddi, don't
                                // want to introduce them in MEA to avoid confusion, and put an assert
                                // here to guard that there is new case we need handle in the future.
                                assert(0);

                            }
                        }
                    }
                    put_ui(bs, 1 /*used_by_curr_pic_s0_flag*/, 1);
                }

                prev = 0;
                // this is the second big 'for' block
                for (i = 0; i < slice_header->strp.num_positive_pics; i++) {
                    // Non-BPyramid GOP
                    if (num_active_ref_p > 1) {
                        // MultiRef Case
                        if (frame_cnt_in_gop < gop_ref_distance) {
                            int DeltaPoc = (int)(gop_ref_distance - frame_cnt_in_gop);
                            put_ue(bs, DeltaPoc - prev - 1 /*delta_poc_s1_minus1*/);
                        } else if (frame_cnt_in_gop > gop_ref_distance) {
                            int DeltaPoc = (int)(gop_ref_distance * slice_header->strp.num_negative_pics - frame_cnt_in_gop);
                            put_ue(bs, DeltaPoc - prev - 1 /*delta_poc_s1_minus1*/);
                        }
                    } else {
                        //  the big 'if' wraps here is -
                        //     if (!slice_header->short_term_ref_pic_set_sps_flag)
                        // From the Teddi logic, the short_term_ref_pic_set_sps_flag only can be '0'
                        // either for B-Prymid or first several frames in a GOP in multi-ref cases
                        // when there are not enough backward refs.
                        // So though there are really some codes under this 'else'in Teddi, don't
                        // want to introduce them in MEA to avoid confusion, and put an assert
                        // here to guard that there is new case we need handle in the future.
                        assert(0);
                    }
                    put_ui(bs, 1 /*used_by_curr_pic_s1_flag*/, 1);
                }
            } else if (sps->num_short_term_ref_pic_sets > 1)
                put_ui(bs, slice_header->short_term_ref_pic_set_idx,
                       (uint8_t)(ceil(log(sps->num_short_term_ref_pic_sets) / log(2.0))));

            if (sps->long_term_ref_pics_present_flag) {
                if (sps->num_long_term_ref_pics_sps > 0)
                    put_ue(bs, slice_header->num_long_term_sps);

                put_ue(bs, slice_header->num_long_term_pics);
            }

            if (slice_header->slice_temporal_mvp_enabled_flag)
                put_ui(bs, slice_header->slice_temporal_mvp_enabled_flag, 1);

        }

        if (sps->sample_adaptive_offset_enabled_flag) {
            put_ui(bs, slice_header->slice_sao_luma_flag, 1);
            put_ui(bs, slice_header->slice_sao_chroma_flag, 1);
        }

        if (slice_header->slice_type != SLICE_I) {
            put_ui(bs, slice_header->num_ref_idx_active_override_flag, 1);

            if (slice_header->num_ref_idx_active_override_flag) {
                put_ue(bs, slice_header->num_ref_idx_l0_active_minus1);
                if (slice_header->slice_type == SLICE_B)
                    put_ue(bs, slice_header->num_ref_idx_l1_active_minus1);
            }

            if (pps->lists_modification_present_flag &&  slice_header->num_poc_total_cur > 1) {
                /* ref_pic_list_modification */
                put_ui(bs, slice_header->ref_pic_list_modification_flag_l0, 1);

                if (slice_header->ref_pic_list_modification_flag_l0) {
                    for (i = 0; i <= slice_header->num_ref_idx_l0_active_minus1; i++) {
                        put_ui(bs, slice_header->list_entry_l0[i],
                               (uint8_t)(ceil(log(slice_header->num_poc_total_cur) / log(2.0))));
                    }
                }

                put_ui(bs, slice_header->ref_pic_list_modification_flag_l1, 1);

                if (slice_header->ref_pic_list_modification_flag_l1) {
                    for (i = 0; i <= slice_header->num_ref_idx_l1_active_minus1; i++) {
                        put_ui(bs, slice_header->list_entry_l1[i],
                               (uint8_t)(ceil(log(slice_header->num_poc_total_cur) / log(2.0))));
                    }
                }
            }

            if (slice_header->slice_type == SLICE_B) {
                put_ui(bs, slice_header->mvd_l1_zero_flag, 1);
            }

            if (pps->cabac_init_present_flag) {
                put_ui(bs, slice_header->cabac_init_present_flag, 1);
            }

            if (slice_header->slice_temporal_mvp_enabled_flag) {
                int collocated_from_l0_flag = 1;

                if (slice_header->slice_type == SLICE_B) {
                    collocated_from_l0_flag = slice_header->collocated_from_l0_flag;
                    put_ui(bs, slice_header->collocated_from_l0_flag, 1);
                }

                if (((collocated_from_l0_flag && (slice_header->num_ref_idx_l0_active_minus1 > 0)) ||
                     (!collocated_from_l0_flag && (slice_header->num_ref_idx_l1_active_minus1 > 0)))) {
                    put_ue(bs, slice_header->collocated_ref_idx);
                }
            }

            put_ue(bs, slice_header->five_minus_max_num_merge_cand);
        }

        put_se(bs, slice_header->slice_qp_delta);

        if (pps->chroma_qp_offset_list_enabled_flag) {
            put_se(bs, slice_header->slice_qp_delta_cb);
            put_se(bs, slice_header->slice_qp_delta_cr);
        }

        if (pps->deblocking_filter_override_enabled_flag) {
            put_ui(bs, slice_header->deblocking_filter_override_flag, 1);
        }
        if (slice_header->deblocking_filter_override_flag) {
            put_ui(bs, slice_header->disable_deblocking_filter_flag, 1);

            if (!slice_header->disable_deblocking_filter_flag) {
                put_se(bs, slice_header->beta_offset_div2);
                put_se(bs, slice_header->tc_offset_div2);
            }
        }

        if (pps->pps_loop_filter_across_slices_enabled_flag &&
            (slice_header->slice_sao_luma_flag || slice_header->slice_sao_chroma_flag ||
             !slice_header->disable_deblocking_filter_flag)) {
            put_ui(bs, slice_header->slice_loop_filter_across_slices_enabled_flag, 1);
        }

    }

    if ((pps->tiles_enabled_flag) || (pps->entropy_coding_sync_enabled_flag)) {
        put_ue(bs, slice_header->num_entry_point_offsets);

        if (slice_header->num_entry_point_offsets > 0) {
            put_ue(bs, slice_header->offset_len_minus1);
        }
    }

    if (pps->slice_segment_header_extension_present_flag) {
        int slice_header_extension_length = 0;

        put_ue(bs, slice_header_extension_length);

        for (i = 0; i < slice_header_extension_length; i++) {
            int slice_header_extension_data_byte = 0;
            put_ui(bs, slice_header_extension_data_byte, 8);
        }
    }
}

static int
build_packed_pic_buffer(struct vaapi_hevc_encodec *data, unsigned char **header_buffer)
{
    bitstream bs;

    bitstream_start(&bs);
    nal_start_code_prefix(&bs, NALU_PPS);
    nal_header(&bs, NALU_PPS);
    pps_rbsp(data, &bs);
    rbsp_trailing_bits(&bs);
    bitstream_end(&bs);

    *header_buffer = (unsigned char *)bs.buffer;
    return bs.bit_offset;
}
static int
build_packed_video_buffer(struct vaapi_hevc_encodec *data, unsigned char **header_buffer)
{
    bitstream bs;

    bitstream_start(&bs);
    nal_start_code_prefix(&bs, NALU_VPS);
    nal_header(&bs, NALU_VPS);
    vps_rbsp(data, &bs);
    rbsp_trailing_bits(&bs);
    bitstream_end(&bs);

    *header_buffer = (unsigned char *)bs.buffer;
    return bs.bit_offset;
}

static int
build_packed_seq_buffer(struct vaapi_hevc_encodec *data, unsigned char **header_buffer)
{
    bitstream bs;

    bitstream_start(&bs);
    nal_start_code_prefix(&bs, NALU_SPS);
    nal_header(&bs, NALU_SPS);
    sps_rbsp(data, &bs);
    rbsp_trailing_bits(&bs);
    bitstream_end(&bs);

    *header_buffer = (unsigned char *)bs.buffer;
    return bs.bit_offset;
}

static int build_packed_slice_buffer(struct vaapi_hevc_encodec *data, unsigned char **header_buffer)
{
    bitstream bs;
    int is_idr = !!data->pic_param.pic_fields.bits.idr_pic_flag;
    int naluType = is_idr ? NALU_IDR_W_DLP : NALU_TRAIL_R;

    bitstream_start(&bs);
    nal_start_code_prefix(&bs, NALU_TRAIL_R);
    nal_header(&bs, naluType);
    sliceHeader_rbsp(&bs, &data->ssh, &data->sps, &data->pps, 0);
    rbsp_trailing_bits(&bs);
    bitstream_end(&bs);

    *header_buffer = (unsigned char *)bs.buffer;
    return bs.bit_offset;
}

/*
  Assume frame sequence is: Frame#0,#1,#2,...,#M,...,#X,... (encoding order)
  1) period between Frame #X and Frame #N = #X - #N
  2) 0 means infinite for intra_period/intra_idr_period, and 0 is invalid for ip_period
  3) intra_idr_period % intra_period (intra_period > 0) and (intra_period -1)% ip_period must be 0
  4) intra_period and intra_idr_period take precedence over ip_period
  5) if ip_period > 1, intra_period and intra_idr_period are not  the strict periods
     of I/IDR frames, see bellow examples
  -------------------------------------------------------------------
  intra_period intra_idr_period ip_period frame sequence (intra_period/intra_idr_period/ip_period)
  0            ignored          1          IDRPPPPPPP ...     (No IDR/I any more)
  0            ignored        >=2          IDR(PBB)(PBB)...   (No IDR/I any more)
  1            0                ignored    IDRIIIIIII...      (No IDR any more)
  1            1                ignored    IDR IDR IDR IDR...
  1            >=2              ignored    IDRII IDRII IDR... (1/3/ignore)
  >=2          0                1          IDRPPP IPPP I...   (3/0/1)
  >=2          0              >=2          IDR(PBB)(PBB)(IBB) (7/0/3)
                                              (PBB)(IBB)(PBB)(IBB)...
  >=2          >=2              1          IDRPPPPP IPPPPP IPPPPP (7/14/1)
                                           IDRPPPPP IPPPPP IPPPPP...
  >=2          >=2              >=2        {IDR(PBB)(PBB)(IBB)(PBB)(IBB)(PBB)} (7/14/3)
                                           {IDR(PBB)(PBB)(IBB)(PBB)(IBB)(PBB)}...
                                           {IDR(PBB)(PBB)(IBB)(PBB)}           (7/14/3)
                                           {IDR(PBB)(PBB)(IBB)(PBB)}...
                                           {IDR(PBB)(PBB)}                     (7/7/3)
                                           {IDR(PBB)(PBB)}.
*/

/*
 * Return displaying order with specified periods and encoding order
 * displaying_order: displaying order
 * frame_type: frame type
 */
void encoding2display_order(
    unsigned long long encoding_order, int intra_period,
    int intra_idr_period, int ip_period,
    unsigned long long *displaying_order,
    int *frame_type)
{
    int encoding_order_gop = 0;

    if (intra_period == 1) { /* all are I/IDR frames */
        *displaying_order = encoding_order;
        if (intra_idr_period == 0)
            *frame_type = (encoding_order == 0) ? FRAME_IDR : FRAME_I;
        else
            *frame_type = (encoding_order % intra_idr_period == 0) ? FRAME_IDR : FRAME_I;
        return;
    }

    if (intra_period == 0)
        intra_idr_period = 0;

    /* new sequence like
     * IDR PPPPP IPPPPP
     * IDR (PBB)(PBB)(IBB)(PBB)
     */
    encoding_order_gop = (intra_idr_period == 0) ? encoding_order :
                         (encoding_order % (intra_idr_period + ((ip_period == 1) ? 0 : 1)));

    if (encoding_order_gop == 0) { /* the first frame */
        *frame_type = FRAME_IDR;
        *displaying_order = encoding_order;
    } else if (((encoding_order_gop - 1) % ip_period) != 0) { /* B frames */
        *frame_type = FRAME_B;
        *displaying_order = encoding_order - 1;
    } else if ((intra_period != 0) && /* have I frames */
               (encoding_order_gop >= 2) &&
               ((ip_period == 1 && encoding_order_gop % (intra_period - 1) == 0) || /* for IDR PPPPP IPPPP */
                /* for IDR (PBB)(PBB)(IBB) */
                (ip_period >= 2 && ((encoding_order_gop - 1) / ip_period % ((intra_period - 1) / ip_period)) == 0))) {
        *frame_type = FRAME_I;
        *displaying_order = encoding_order + ip_period - 1;
    } else {
        *frame_type = FRAME_P;
        *displaying_order = encoding_order + ip_period - 1;
    }


}


static char *rc_to_string(int rcmode)
{
    switch (rc_mode) {
    case VA_RC_NONE:
        return "NONE";
    case VA_RC_CBR:
        return "CBR";
    case VA_RC_VBR:
        return "VBR";
    case VA_RC_VCM:
        return "VCM";
    case VA_RC_CQP:
        return "CQP";
    case VA_RC_VBR_CONSTRAINED:
        return "VBR_CONSTRAINED";
    default:
        return "Unknown";
    }
}

// static int process_cmdline(int argc, char *argv[])
// {
//     int c;
//     const struct option long_opts[] = {
//         {"help", no_argument, NULL, 0 },
//         {"bitrate", required_argument, NULL, 1 },
//         {"minqp", required_argument, NULL, 2 },
//         {"initialqp", required_argument, NULL, 3 },
//         {"intra_period", required_argument, NULL, 4 },
//         {"idr_period", required_argument, NULL, 5 },
//         {"ip_period", required_argument, NULL, 6 },
//         {"rcmode", required_argument, NULL, 7 },
//         {"srcyuv", required_argument, NULL, 9 },
//         {"recyuv", required_argument, NULL, 10 },
//         {"fourcc", required_argument, NULL, 11 },
//         {"syncmode", no_argument, NULL, 12 },
//         {"enablePSNR", no_argument, NULL, 13 },
//         {"prit", required_argument, NULL, 14 },
//         {"priv", required_argument, NULL, 15 },
//         {"framecount", required_argument, NULL, 16 },
//         {"profile", required_argument, NULL, 17 },
//         {"p2b", required_argument, NULL, 18 },
//         {"lowpower", required_argument, NULL, 19 },
//         {NULL, no_argument, NULL, 0 }
//     };
//     int long_index;

//     while ((c = getopt_long_only(argc, argv, "w:h:n:f:o:?", long_opts, &long_index)) != EOF) {
//         switch (c) {
//         case 'w':
//             data->frame_width = atoi(optarg);
//             break;
//         case 'h':
//             data->frame_height = atoi(optarg);
//             break;
//         case 'n':
//         case 16:
//             frame_count = atoi(optarg);
//             break;
//         case 'f':
//             frame_rate = atoi(optarg);
//             break;
//         case 'o':
//             coded_fn = strdup(optarg);
//             break;
//         case 0:
//             print_help();
//             exit(0);
//         case 1:
//             frame_bitrate = atoi(optarg)*1000;
//             break;
//         case 2:
//             minimal_qp = atoi(optarg);
//             break;
//         case 3:
//             initial_qp = atoi(optarg);
//             break;
//         case 4:
//             intra_period = atoi(optarg);
//             break;
//         case 5:
//             intra_idr_period = atoi(optarg);
//             break;
//         case 6:
//             ip_period = atoi(optarg);
//             break;
//         case 7:
//             rc_mode = string_to_rc(optarg);
//             if (rc_mode < 0) {
//                 print_help();
//                 exit(1);
//             }
//             break;
//         case 9:
//             srcyuv_fn = strdup(optarg);
//             break;
//         case 10:
//             recyuv_fn = strdup(optarg);
//             break;
//         case 11:
//             srcyuv_fourcc = string_to_fourcc(optarg);
//             if (srcyuv_fourcc <= 0) {
//                 print_help();
//                 exit(1);
//             }
//             break;
//         case 12:
//             break;
//         case 13:
//             calc_psnr = 1;
//             break;
//         case 14:
//             misc_priv_type = strtol(optarg, NULL, 0);
//             break;
//         case 15:
//             misc_priv_value = strtol(optarg, NULL, 0);
//             break;
//         case 17:
//             if (strncmp(optarg, "1", 1) == 0) {
//                 real_hevc_profile = 1;
//                 hevc_profile = VAProfileHEVCMain;
//             } else if (strncmp(optarg, "2", 1) == 0) {
//                 real_hevc_profile = 2;
//                 hevc_profile = VAProfileHEVCMain10;
//             } else
//                 hevc_profile = 0;
//             break;
//         case 18:
//             p2b = atoi(optarg);
//             break;
//         case 19:
//             lowpower = atoi(optarg);
//             break;

//         case ':':
//         case '?':
//             print_help();
//             exit(0);
//         }
//     }

//     if (ip_period < 1) {
//         printf(" ip_period must be greater than 0\n");
//         exit(0);
//     }
//     if (intra_period != 1 && (intra_period - 1) % ip_period != 0) {
//         printf(" intra_period -1 must be a multiplier of ip_period\n");
//         exit(0);
//     }
//     if (intra_period != 0 && intra_idr_period % intra_period != 0) {
//         printf(" intra_idr_period must be a multiplier of intra_period\n");
//         exit(0);
//     }
//     if (ip_period > 1) {
//         frame_count -= (frame_count - 1) % ip_period;
//     }

//     if (frame_bitrate == 0)
//         frame_bitrate = (long long int) data->frame_width * data->frame_height * 12 * frame_rate / 50;

//     /* open source file */
//     if (srcyuv_fn) {
//         srcyuv_fp = fopen(srcyuv_fn, "r");

//         if (srcyuv_fp == NULL)
//             printf("Open source YUV file %s failed, use auto-generated YUV data\n", srcyuv_fn);
//         else {
//             struct stat tmp;

//             fstat(fileno(srcyuv_fp), &tmp);
//             srcyuv_frames = tmp.st_size / (data->frame_width * data->frame_height * 1.5);
//             printf("Source YUV file %s with %llu frames\n", srcyuv_fn, srcyuv_frames);

//             if (frame_count == 0)
//                 frame_count = srcyuv_frames;
//         }
//     }

//     /* open source file */
//     if (recyuv_fn) {
//         recyuv_fp = fopen(recyuv_fn, "w+");

//         if (recyuv_fp == NULL)
//             printf("Open reconstructed YUV file %s failed\n", recyuv_fn);
//     }

//     if (coded_fn == NULL) {
//         struct stat buf;
//         if (stat("/tmp", &buf) == 0)
//             coded_fn = strdup("/tmp/test.265");
//         else if (stat("/sdcard", &buf) == 0)
//             coded_fn = strdup("/sdcard/test.265");
//         else
//             coded_fn = strdup("./test.265");
//     }

//     /* store coded data into a file */
//     if (coded_fn) {
//         coded_fp = fopen(coded_fn, "w+");
//     } else {
//         printf("Copy file string failed");
//         exit(1);
//     }
//     if (coded_fp == NULL) {
//         printf("Open file %s failed, exit\n", coded_fn);
//         exit(1);
//     }

//     frame_hor_stride = (data->frame_width + 63) & (~63);
//     frame_ver_stride = (data->frame_height + 63) & (~63);
//     if (data->frame_width != frame_hor_stride ||
//         data->frame_height != frame_ver_stride) {
//         printf("Source frame is %dx%d and will code clip to %dx%d with crop\n",
//                data->frame_width, data->frame_height,
//                frame_hor_stride, frame_ver_stride
//               );
//     }

//     return 0;
// }

static int init_va(struct vaapi_hevc_encodec *data)
{
    VAProfile profile_list[] = {VAProfileHEVCMain, VAProfileHEVCMain10};
    VAEntrypoint *entrypoints;
    int num_entrypoints, slice_entrypoint;
    int support_encode = 0;
    int major_ver, minor_ver;
    int ret = -1;
    VAStatus va_status;
    unsigned int i;

    data->va_dpy = va_open_display();
    va_status = vaInitialize(data->va_dpy, &major_ver, &minor_ver);
    CHECK_VASTATUS(va_status, "vaInitialize");

    num_entrypoints = vaMaxNumEntrypoints(data->va_dpy);
    entrypoints = malloc(num_entrypoints * sizeof(*entrypoints));
    if (!entrypoints) {
        func_error("calloc fail, check free memery.");
        goto FAIL1;
    }

    /* use the highest profile */
    for (i = 0; i < sizeof(profile_list) / sizeof(profile_list[0]); i++) {
        if ((data->hevc_profile != ~0) && data->hevc_profile != profile_list[i])
            continue;

        data->hevc_profile = profile_list[i];
        vaQueryConfigEntrypoints(data->va_dpy, data->hevc_profile, entrypoints, &num_entrypoints);
        for (slice_entrypoint = 0; slice_entrypoint < num_entrypoints; slice_entrypoint++) {
            if (entrypoints[slice_entrypoint] == VAEntrypointEncSlice ||
                entrypoints[slice_entrypoint] == VAEntrypointEncSliceLP ) {
                support_encode = 1;
                break;
            }
        }
        if (support_encode == 1)
            break;
    }

    if (support_encode == 0) {
        func_error("Can't find VAEntrypointEncSlice for HEVC profiles\n");
        goto FAIL2;
    } else {
        switch (data->hevc_profile) {
        case VAProfileHEVCMain:
            data->hevc_profile = VAProfileHEVCMain;
            log_info("Use profile VAProfileHEVCMain\n");
            break;

        case VAProfileHEVCMain10:
            data->hevc_profile = VAProfileHEVCMain10;
            log_info("Use profile VAProfileHEVCMain10\n");
            break;
        default:
            log_info("unknow profile. Set to Main");
            data->hevc_profile = VAProfileHEVCMain;
            constraint_set_flag |= (1 << 0 | 1 << 1); /* Annex A.2.1 & A.2.2 */
            ip_period = 1;
            break;
        }
    }

    /* find out the format for the render target, and rate control mode */
    for (i = 0; i < VAConfigAttribTypeMax; i++)
        attrib[i].type = i;

    if (lowpower)
    {
        data->entryPoint = VAEntrypointEncSliceLP;
        LCU_SIZE = 64;
    }

    va_status = vaGetConfigAttributes(data->va_dpy, data->hevc_profile, data->entryPoint,
                                      &attrib[0], VAConfigAttribTypeMax);
    CHECK_VASTATUS(va_status, "vaGetConfigAttributes");
    /* check the interested configattrib */
    if ((attrib[VAConfigAttribRTFormat].value & VA_RT_FORMAT_YUV420) == 0) {
        func_error("Not find desired YUV420 RT format\n");
        goto FAIL2;
    } else {
        data->config_attrib[data->config_attrib_num].type = VAConfigAttribRTFormat;
        data->config_attrib[data->config_attrib_num].value = VA_RT_FORMAT_YUV420;
        data->config_attrib_num++;
    }

    if (attrib[VAConfigAttribRateControl].value != VA_ATTRIB_NOT_SUPPORTED) {
        int tmp = attrib[VAConfigAttribRateControl].value;

        log_info("Support rate control mode (0x%x):", tmp);

        if (tmp & VA_RC_NONE)
            log_info("NONE ");
        if (tmp & VA_RC_CBR)
            log_info("CBR ");
        if (tmp & VA_RC_VBR)
            log_info("VBR ");
        if (tmp & VA_RC_VCM)
            log_info("VCM ");
        if (tmp & VA_RC_CQP)
            log_info("CQP ");
        if (tmp & VA_RC_VBR_CONSTRAINED)
            log_info("VBR_CONSTRAINED ");

        if (rc_mode == -1 || !(rc_mode & tmp))  {
            if (rc_mode != -1) {
                log_warning("Warning: Don't support the specified RateControl mode: %s!!!, switch to ", rc_to_string(rc_mode));
            }

            for (i = 0; i < sizeof(rc_default_modes) / sizeof(rc_default_modes[0]); i++) {
                if (rc_default_modes[i] & tmp) {
                    rc_mode = rc_default_modes[i];
                    break;
                }
            }

            log_info("RateControl mode: %s", rc_to_string(rc_mode));
        }

        data->config_attrib[data->config_attrib_num].type = VAConfigAttribRateControl;
        data->config_attrib[data->config_attrib_num].value = rc_mode;
        data->config_attrib_num++;
    }


    if (attrib[VAConfigAttribEncPackedHeaders].value != VA_ATTRIB_NOT_SUPPORTED) {
        int tmp = attrib[VAConfigAttribEncPackedHeaders].value;

        log_info("Support VAConfigAttribEncPackedHeaders");

        hevc_packedheader = 1;
        data->config_attrib[data->config_attrib_num].type = VAConfigAttribEncPackedHeaders;
        data->config_attrib[data->config_attrib_num].value = VA_ENC_PACKED_HEADER_NONE;

        if (tmp & VA_ENC_PACKED_HEADER_SEQUENCE) {
            log_info("Support packed sequence headers");
            data->config_attrib[data->config_attrib_num].value |= VA_ENC_PACKED_HEADER_SEQUENCE;
        }

        if (tmp & VA_ENC_PACKED_HEADER_PICTURE) {
            log_info("Support packed picture headers");
            data->config_attrib[data->config_attrib_num].value |= VA_ENC_PACKED_HEADER_PICTURE;
        }

        if (tmp & VA_ENC_PACKED_HEADER_SLICE) {
            log_info("Support packed slice headers");
            data->config_attrib[data->config_attrib_num].value |= VA_ENC_PACKED_HEADER_SLICE;
        }

        if (tmp & VA_ENC_PACKED_HEADER_MISC) {
            log_info("Support packed misc headers");
            data->config_attrib[data->config_attrib_num].value |= VA_ENC_PACKED_HEADER_MISC;
        }

        enc_packed_header_idx = data->config_attrib_num;
        data->config_attrib_num++;
    }

    if (attrib[VAConfigAttribEncInterlaced].value != VA_ATTRIB_NOT_SUPPORTED) {
        int tmp = attrib[VAConfigAttribEncInterlaced].value;

        log_info("Support VAConfigAttribEncInterlaced");

        if (tmp & VA_ENC_INTERLACED_FRAME)
            log_info("support VA_ENC_INTERLACED_FRAME");
        if (tmp & VA_ENC_INTERLACED_FIELD)
            log_info("Support VA_ENC_INTERLACED_FIELD");
        if (tmp & VA_ENC_INTERLACED_MBAFF)
            log_info("Support VA_ENC_INTERLACED_MBAFF");
        if (tmp & VA_ENC_INTERLACED_PAFF)
            log_info("Support VA_ENC_INTERLACED_PAFF");

        data->config_attrib[data->config_attrib_num].type = VAConfigAttribEncInterlaced;
        data->config_attrib[data->config_attrib_num].value = VA_ENC_PACKED_HEADER_NONE;
        data->config_attrib_num++;
    }

    if (attrib[VAConfigAttribEncMaxRefFrames].value != VA_ATTRIB_NOT_SUPPORTED) {
        hevc_maxref = attrib[VAConfigAttribEncMaxRefFrames].value;

        log_info("Support %d RefPicList0 and %d RefPicList1",
               hevc_maxref & 0xffff, (hevc_maxref >> 16) & 0xffff);
    }

    if (attrib[VAConfigAttribEncMaxSlices].value != VA_ATTRIB_NOT_SUPPORTED)
        log_info("Support %d slices", attrib[VAConfigAttribEncMaxSlices].value);

    if (attrib[VAConfigAttribEncSliceStructure].value != VA_ATTRIB_NOT_SUPPORTED) {
        int tmp = attrib[VAConfigAttribEncSliceStructure].value;

        log_info("Support VAConfigAttribEncSliceStructure");

        if (tmp & VA_ENC_SLICE_STRUCTURE_ARBITRARY_ROWS)
            log_info("Support VA_ENC_SLICE_STRUCTURE_ARBITRARY_ROWS");
        if (tmp & VA_ENC_SLICE_STRUCTURE_POWER_OF_TWO_ROWS)
            log_info("Support VA_ENC_SLICE_STRUCTURE_POWER_OF_TWO_ROWS");
        if (tmp & VA_ENC_SLICE_STRUCTURE_ARBITRARY_MACROBLOCKS)
            log_info("Support VA_ENC_SLICE_STRUCTURE_ARBITRARY_MACROBLOCKS");
    }
    if (attrib[VAConfigAttribEncMacroblockInfo].value != VA_ATTRIB_NOT_SUPPORTED) {
        log_info("Support VAConfigAttribEncMacroblockInfo");
    }

    free(entrypoints);

    return ret;

FAIL2:
    free(entrypoints);
FAIL1:
    va_close_display(data->va_dpy);
    return ret;
}

static int setup_encode(struct vaapi_hevc_encodec *data)
{
    VAStatus va_status;
    VASurfaceID *tmp_surfaceid;
    int codedbuf_size, i;

    va_status = vaCreateConfig(data->va_dpy, data->hevc_profile, data->entryPoint,
                               &data->config_attrib[0], data->config_attrib_num, &data->config_id);
    CHECK_VASTATUS(va_status, "vaCreateConfig");

    /* create source surfaces */
    va_status = vaCreateSurfaces(data->va_dpy,
                                 VA_RT_FORMAT_YUV420, data->frame_hor_stride, data->frame_ver_stride,
                                 &data->src_surface[0], SURFACE_NUM,
                                 NULL, 0);
    CHECK_VASTATUS(va_status, "vaCreateSurfaces");

    /* create reference surfaces */
    va_status = vaCreateSurfaces(
                    data->va_dpy,
                    VA_RT_FORMAT_YUV420, data->frame_hor_stride, data->frame_ver_stride,
                    &data->ref_surface[0], SURFACE_NUM,
                    NULL, 0
                );
    CHECK_VASTATUS(va_status, "vaCreateSurfaces");

    tmp_surfaceid = calloc(2 * SURFACE_NUM, sizeof(VASurfaceID));
    if (tmp_surfaceid) {
        memcpy(tmp_surfaceid, data->src_surface, SURFACE_NUM * sizeof(VASurfaceID));
        memcpy(tmp_surfaceid + SURFACE_NUM, data->ref_surface, SURFACE_NUM * sizeof(VASurfaceID));
    }

    /* Create a context for this encode pipe */
    va_status = vaCreateContext(data->va_dpy, data->config_id,
                                data->frame_hor_stride, data->frame_ver_stride,
                                VA_PROGRESSIVE,
                                tmp_surfaceid, 2 * SURFACE_NUM,
                                &data->context_id);
    CHECK_VASTATUS(va_status, "vaCreateContext");
    free(tmp_surfaceid);

    codedbuf_size = ((long long int) data->frame_hor_stride * data->frame_ver_stride * 400) / (16 * 16);

    for (i = 0; i < SURFACE_NUM; i++) {
        /* create coded buffer once for all
         * other VA buffers which won't be used again after vaRenderPicture.
         * so APP can always vaCreateBuffer for every frame
         * but coded buffer need to be mapped and accessed after vaRenderPicture/vaEndPicture
         * so VA won't maintain the coded buffer
         */
        va_status = vaCreateBuffer(data->va_dpy, data->context_id, VAEncCodedBufferType,
                                   codedbuf_size, 1, NULL, &data->pkt_buf[i]);
        CHECK_VASTATUS(va_status, "vaCreateBuffer");
    }

    return 0;
}

#define partition(ref, field, key, ascending)   \
    while (i <= j) {                            \
        if (ascending) {                        \
            while (ref[i].field < key)          \
                i++;                            \
            while (ref[j].field > key)          \
                j--;                            \
        } else {                                \
            while (ref[i].field > key)          \
                i++;                            \
            while (ref[j].field < key)          \
                j--;                            \
        }                                       \
        if (i <= j) {                           \
            tmp = ref[i];                       \
            ref[i] = ref[j];                    \
            ref[j] = tmp;                       \
            i++;                                \
            j--;                                \
        }                                       \
    }                                           \

static void sort_one(VAPictureHEVC ref[], int left, int right,
                     int ascending)
{
    VAPictureHEVC tmp;
    int i = left, j = right;
    unsigned int key = ref[(left + right) / 2].pic_order_cnt;
    partition(ref, pic_order_cnt, (signed int)key, ascending);

    /* recursion */
    if (left < j)
        sort_one(ref, left, j, ascending);

    if (i < right)
        sort_one(ref, i, right, ascending);
}

static void sort_two(VAPictureHEVC ref[], int left, int right, unsigned int key,
                     int partition_ascending, int list0_ascending, int list1_ascending)
{
    VAPictureHEVC tmp;
    int i = left, j = right;

    partition(ref, pic_order_cnt, (signed int)key, partition_ascending);

    sort_one(ref, left, i - 1, list0_ascending);
    sort_one(ref, j + 1, right, list1_ascending);
}

static int update_ReferenceFrames(struct vaapi_hevc_encodec *data)
{
    int i;

    if (data->current_frame_type == FRAME_B)
        return 0;

    data->numShortTerm++;
    if (data->numShortTerm > num_ref_frames)
        data->numShortTerm = num_ref_frames;
    for (i = data->numShortTerm - 1; i > 0; i--)
        ReferenceFrames[i] = ReferenceFrames[i - 1];
    ReferenceFrames[0] = CurrentCurrPic;

    return 0;
}

static int update_RefPicList(struct vaapi_hevc_encodec *data)
{
    unsigned int current_poc = CurrentCurrPic.pic_order_cnt;

    if (data->current_frame_type == FRAME_P) {
        memcpy(RefPicList0_P, ReferenceFrames, data->numShortTerm * sizeof(VAPictureHEVC));
        sort_one(RefPicList0_P, 0, data->numShortTerm - 1, 0);
    }

    if (data->current_frame_type == FRAME_B) {
        memcpy(RefPicList0_B, ReferenceFrames, data->numShortTerm * sizeof(VAPictureHEVC));
        sort_two(RefPicList0_B, 0, data->numShortTerm - 1, current_poc, 1, 0, 1);

        memcpy(RefPicList1_B, ReferenceFrames, data->numShortTerm * sizeof(VAPictureHEVC));
        sort_two(RefPicList1_B, 0, data->numShortTerm - 1, current_poc, 0, 1, 0);
    }

    return 0;
}


static int render_sequence(struct vaapi_hevc_encodec *data)
{

    VABufferID seq_param_buf = VA_INVALID_ID;
    VABufferID rc_param_buf = VA_INVALID_ID;
    VABufferID misc_param_tmpbuf = VA_INVALID_ID;
    VABufferID render_id[2] = {VA_INVALID_ID};
    VAStatus va_status;
    VAEncMiscParameterBuffer *misc_param, *misc_param_tmp;
    VAEncMiscParameterRateControl *misc_rate_ctrl;
    data->seq_param.general_profile_idc = data->sps.ptps.general_profile_idc;
    data->seq_param.general_level_idc = data->sps.ptps.general_level_idc;
    data->seq_param.general_tier_flag = (uint8_t)(data->sps.ptps.general_tier_flag);

    data->seq_param.intra_period = intra_period;
    data->seq_param.intra_idr_period = intra_idr_period;
    data->seq_param.ip_period = ip_period;

    data->seq_param.bits_per_second = frame_bitrate;
    data->seq_param.pic_width_in_luma_samples = data->sps.pic_width_in_luma_samples;
    data->seq_param.pic_height_in_luma_samples = data->sps.pic_height_in_luma_samples;

    data->seq_param.seq_fields.bits.chroma_format_idc = 1;
    data->seq_param.seq_fields.bits.separate_colour_plane_flag = 0;
    data->seq_param.seq_fields.bits.bit_depth_luma_minus8 = data->sps.bit_depth_luma_minus8;
    data->seq_param.seq_fields.bits.bit_depth_chroma_minus8 = data->sps.bit_depth_chroma_minus8;
    data->seq_param.seq_fields.bits.scaling_list_enabled_flag = data->sps.scaling_list_enabled_flag;
    data->seq_param.seq_fields.bits.strong_intra_smoothing_enabled_flag = data->sps.strong_intra_smoothing_enabled_flag;
    data->seq_param.seq_fields.bits.amp_enabled_flag = data->sps.amp_enabled_flag;
    data->seq_param.seq_fields.bits.sample_adaptive_offset_enabled_flag = data->sps.sample_adaptive_offset_enabled_flag;
    data->seq_param.seq_fields.bits.pcm_enabled_flag = data->sps.pcm_enabled_flag;
    data->seq_param.seq_fields.bits.pcm_loop_filter_disabled_flag = data->sps.pcm_loop_filter_disabled_flag;
    data->seq_param.seq_fields.bits.sps_temporal_mvp_enabled_flag = data->sps.sps_temporal_mvp_enabled_flag;

    data->seq_param.log2_min_luma_coding_block_size_minus3 = data->sps.log2_min_luma_coding_block_size_minus3;
    data->seq_param.log2_diff_max_min_luma_coding_block_size = data->sps.log2_diff_max_min_luma_coding_block_size;
    data->seq_param.log2_min_transform_block_size_minus2 = data->sps.log2_min_luma_transform_block_size_minus2;
    data->seq_param.log2_diff_max_min_transform_block_size = data->sps.log2_diff_max_min_luma_transform_block_size;
    data->seq_param.max_transform_hierarchy_depth_inter = data->sps.max_transform_hierarchy_depth_inter;
    data->seq_param.max_transform_hierarchy_depth_intra = data->sps.max_transform_hierarchy_depth_intra;

    data->seq_param.vui_parameters_present_flag = data->sps.vui_parameters_present_flag;

    va_status = vaCreateBuffer(data->va_dpy, data->context_id,
                               VAEncSequenceParameterBufferType,
                               sizeof(data->seq_param), 1, &data->seq_param, &seq_param_buf);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    va_status = vaCreateBuffer(data->va_dpy, data->context_id,
                               VAEncMiscParameterBufferType,
                               sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterRateControl),
                               1, NULL, &rc_param_buf);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    vaMapBuffer(data->va_dpy, rc_param_buf, (void **)&misc_param);
    misc_param->type = VAEncMiscParameterTypeRateControl;
    misc_rate_ctrl = (VAEncMiscParameterRateControl *)misc_param->data;
    memset(misc_rate_ctrl, 0, sizeof(*misc_rate_ctrl));
    misc_rate_ctrl->bits_per_second = frame_bitrate;
    misc_rate_ctrl->target_percentage = 66;
    misc_rate_ctrl->window_size = 1000;
    misc_rate_ctrl->initial_qp = initial_qp;
    misc_rate_ctrl->min_qp = minimal_qp;
    misc_rate_ctrl->basic_unit_size = 0;
    vaUnmapBuffer(data->va_dpy, rc_param_buf);

    render_id[0] = seq_param_buf;
    render_id[1] = rc_param_buf;

    va_status = vaRenderPicture(data->va_dpy, data->context_id, &render_id[0], 2);
    CHECK_VASTATUS(va_status, "vaRenderPicture");
    if (seq_param_buf != VA_INVALID_ID) {
        vaDestroyBuffer(data->va_dpy, seq_param_buf);
        seq_param_buf = VA_INVALID_ID;
    }

    if (rc_param_buf != VA_INVALID_ID) {
        vaDestroyBuffer(data->va_dpy, rc_param_buf);
        rc_param_buf = VA_INVALID_ID;
    }


    if (misc_priv_type != 0) {
        va_status = vaCreateBuffer(data->va_dpy, data->context_id,
                                   VAEncMiscParameterBufferType,
                                   sizeof(VAEncMiscParameterBuffer),
                                   1, NULL, &misc_param_tmpbuf);
        CHECK_VASTATUS(va_status, "vaCreateBuffer");
        vaMapBuffer(data->va_dpy, misc_param_tmpbuf, (void **)&misc_param_tmp);
        misc_param_tmp->type = misc_priv_type;
        misc_param_tmp->data[0] = misc_priv_value;
        vaUnmapBuffer(data->va_dpy, misc_param_tmpbuf);

        va_status = vaRenderPicture(data->va_dpy, data->context_id, &misc_param_tmpbuf, 1);
    }

    return 0;
}

static int render_picture(struct vaapi_hevc_encodec *data)
{
    VABufferID pic_param_buf = VA_INVALID_ID;
    VAStatus va_status;
    int i = 0;

    memcpy(data->pic_param.reference_frames, ReferenceFrames, data->numShortTerm * sizeof(VAPictureHEVC));
    for (i = data->numShortTerm; i < SURFACE_NUM; i++) {
        data->pic_param.reference_frames[i].picture_id = VA_INVALID_SURFACE;
        data->pic_param.reference_frames[i].flags = VA_PICTURE_HEVC_INVALID;
    }

    data->pic_param.last_picture = 0;
    data->pic_param.last_picture |= ((data->current_frame_encoding + 1) % intra_period == 0) ? HEVC_LAST_PICTURE_EOSEQ : 0;
    data->pic_param.last_picture |= ((data->current_frame_encoding + 1) == frame_count) ? HEVC_LAST_PICTURE_EOSTREAM : 0;
    data->pic_param.coded_buf = data->pkt_buf[current_slot];

    data->pic_param.decoded_curr_pic.picture_id = data->ref_surface[current_slot];
    data->pic_param.decoded_curr_pic.pic_order_cnt = calc_poc(data, (data->current_pkt_idx - data->current_IDR_display) % MaxPicOrderCntLsb) * 2;
    data->pic_param.decoded_curr_pic.flags = 0;
    CurrentCurrPic = data->pic_param.decoded_curr_pic;

    data->pic_param.collocated_ref_pic_index = data->pps.num_ref_idx_l0_default_active_minus1;
    data->pic_param.pic_init_qp = data->pps.init_qp_minus26 + 26;
    data->pic_param.diff_cu_qp_delta_depth = data->pps.diff_cu_qp_delta_depth;
    data->pic_param.pps_cb_qp_offset = data->pps.pps_cb_qp_offset;
    data->pic_param.pps_cr_qp_offset = data->pps.pps_cr_qp_offset;

    data->pic_param.num_tile_columns_minus1 = data->pps.num_tile_columns_minus1;
    data->pic_param.num_tile_rows_minus1 = data->pps.num_tile_rows_minus1;
    for (i = 0; i <= (unsigned int)(data->pic_param.num_tile_columns_minus1); i++) {
        data->pic_param.column_width_minus1[i] = 0;
    }
    for (i = 0; i <= (unsigned int)(data->pic_param.num_tile_rows_minus1); i++) {
        data->pic_param.row_height_minus1[i] = 0;
    }

    data->pic_param.log2_parallel_merge_level_minus2 = data->pps.log2_parallel_merge_level_minus2;
    data->pic_param.ctu_max_bitsize_allowed = 0;
    data->pic_param.num_ref_idx_l0_default_active_minus1 = data->pps.num_ref_idx_l0_default_active_minus1;
    data->pic_param.num_ref_idx_l1_default_active_minus1 = data->pps.num_ref_idx_l1_default_active_minus1;
    data->pic_param.slice_pic_parameter_set_id = 0;
    data->pic_param.pic_fields.bits.idr_pic_flag         = (data->current_frame_type == FRAME_IDR);
    data->pic_param.pic_fields.bits.coding_type          = data->current_frame_type == FRAME_IDR ? FRAME_I : data->current_frame_type;
    data->pic_param.pic_fields.bits.reference_pic_flag   = data->current_frame_type != FRAME_B ? 1 : 0;
    data->pic_param.pic_fields.bits.dependent_slice_segments_enabled_flag = data->pps.dependent_slice_segments_enabled_flag;
    data->pic_param.pic_fields.bits.sign_data_hiding_enabled_flag = data->pps.sign_data_hiding_enabled_flag;
    data->pic_param.pic_fields.bits.constrained_intra_pred_flag = data->pps.constrained_intra_pred_flag;
    data->pic_param.pic_fields.bits.transform_skip_enabled_flag = data->pps.transform_skip_enabled_flag;
    data->pic_param.pic_fields.bits.cu_qp_delta_enabled_flag = data->pps.cu_qp_delta_enabled_flag;
    data->pic_param.pic_fields.bits.weighted_pred_flag = data->pps.weighted_pred_flag;
    data->pic_param.pic_fields.bits.weighted_bipred_flag = data->pps.weighted_bipred_flag;
    data->pic_param.pic_fields.bits.transquant_bypass_enabled_flag = data->pps.transquant_bypass_enabled_flag;
    data->pic_param.pic_fields.bits.tiles_enabled_flag = data->pps.tiles_enabled_flag;
    data->pic_param.pic_fields.bits.entropy_coding_sync_enabled_flag = data->pps.entropy_coding_sync_enabled_flag;
    data->pic_param.pic_fields.bits.loop_filter_across_tiles_enabled_flag = data->pps.loop_filter_across_tiles_enabled_flag;
    data->pic_param.pic_fields.bits.pps_loop_filter_across_slices_enabled_flag = data->pps.pps_loop_filter_across_slices_enabled_flag;
    data->pic_param.pic_fields.bits.scaling_list_data_present_flag = data->pps.pps_scaling_list_data_present_flag;

    va_status = vaCreateBuffer(data->va_dpy, data->context_id, VAEncPictureParameterBufferType,
                               sizeof(data->pic_param), 1, &data->pic_param, &pic_param_buf);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");;

    va_status = vaRenderPicture(data->va_dpy, data->context_id, &pic_param_buf, 1);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    if (pic_param_buf != VA_INVALID_ID) {
        vaDestroyBuffer(data->va_dpy, pic_param_buf);
        pic_param_buf = VA_INVALID_ID;
    }

    return 0;
}

static int render_packedvideo(struct vaapi_hevc_encodec *data)
{

    VAEncPackedHeaderParameterBuffer packedheader_param_buffer;
    VABufferID packedvideo_para_bufid = VA_INVALID_ID;
    VABufferID packedvideo_data_bufid = VA_INVALID_ID;
    VABufferID render_id[2] = {VA_INVALID_ID};
    unsigned int length_in_bits;
    unsigned char *packedvideo_buffer = NULL;
    VAStatus va_status;

    length_in_bits = build_packed_video_buffer(data, &packedvideo_buffer);

    packedheader_param_buffer.type = VAEncPackedHeaderSequence;

    packedheader_param_buffer.bit_length = length_in_bits; /*length_in_bits*/
    packedheader_param_buffer.has_emulation_bytes = 0;
    va_status = vaCreateBuffer(data->va_dpy,
                               data->context_id,
                               VAEncPackedHeaderParameterBufferType,
                               sizeof(packedheader_param_buffer), 1, &packedheader_param_buffer,
                               &packedvideo_para_bufid);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    va_status = vaCreateBuffer(data->va_dpy,
                               data->context_id,
                               VAEncPackedHeaderDataBufferType,
                               (length_in_bits + 7) / 8, 1, packedvideo_buffer,
                               &packedvideo_data_bufid);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    render_id[0] = packedvideo_para_bufid;
    render_id[1] = packedvideo_data_bufid;
    va_status = vaRenderPicture(data->va_dpy, data->context_id, render_id, 2);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    free(packedvideo_buffer);

    if (packedvideo_para_bufid != VA_INVALID_ID) {
        vaDestroyBuffer(data->va_dpy, packedvideo_para_bufid);
        packedvideo_para_bufid = VA_INVALID_ID;
    }
    if (packedvideo_data_bufid != VA_INVALID_ID) {
        vaDestroyBuffer(data->va_dpy, packedvideo_data_bufid);
        packedvideo_data_bufid = VA_INVALID_ID;
    }

    return 0;
}

static int render_packedsequence(struct vaapi_hevc_encodec *data)
{
    VAEncPackedHeaderParameterBuffer packedheader_param_buffer;
    VABufferID packedseq_para_bufid = VA_INVALID_ID;
    VABufferID packedseq_data_bufid = VA_INVALID_ID;
    VABufferID render_id[2] = {VA_INVALID_ID};
    unsigned int length_in_bits;
    unsigned char *packedseq_buffer = NULL;
    VAStatus va_status;

    length_in_bits = build_packed_seq_buffer(data, &packedseq_buffer);

    packedheader_param_buffer.type = VAEncPackedHeaderSequence;

    packedheader_param_buffer.bit_length = length_in_bits; /*length_in_bits*/
    packedheader_param_buffer.has_emulation_bytes = 0;
    va_status = vaCreateBuffer(data->va_dpy,
                               data->context_id,
                               VAEncPackedHeaderParameterBufferType,
                               sizeof(packedheader_param_buffer), 1, &packedheader_param_buffer,
                               &packedseq_para_bufid);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    va_status = vaCreateBuffer(data->va_dpy,
                               data->context_id,
                               VAEncPackedHeaderDataBufferType,
                               (length_in_bits + 7) / 8, 1, packedseq_buffer,
                               &packedseq_data_bufid);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    render_id[0] = packedseq_para_bufid;
    render_id[1] = packedseq_data_bufid;
    va_status = vaRenderPicture(data->va_dpy, data->context_id, render_id, 2);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    free(packedseq_buffer);

    if (packedseq_para_bufid != VA_INVALID_ID) {
        vaDestroyBuffer(data->va_dpy, packedseq_para_bufid);
        packedseq_para_bufid = VA_INVALID_ID;
    }
    if (packedseq_data_bufid != VA_INVALID_ID) {
        vaDestroyBuffer(data->va_dpy, packedseq_data_bufid);
        packedseq_para_bufid = VA_INVALID_ID;
    }

    return 0;
}


static int render_packedpicture(struct vaapi_hevc_encodec *data)
{
    VAEncPackedHeaderParameterBuffer packedheader_param_buffer;
    VABufferID packedpic_para_bufid = VA_INVALID_ID;
    VABufferID packedpic_data_bufid = VA_INVALID_ID;
    VABufferID render_id[2] = {VA_INVALID_ID};
    unsigned int length_in_bits;
    unsigned char *packedpic_buffer = NULL;
    VAStatus va_status;

    length_in_bits = build_packed_pic_buffer(data, &packedpic_buffer);
    packedheader_param_buffer.type = VAEncPackedHeaderPicture;
    packedheader_param_buffer.bit_length = length_in_bits;
    packedheader_param_buffer.has_emulation_bytes = 0;

    va_status = vaCreateBuffer(data->va_dpy,
                               data->context_id,
                               VAEncPackedHeaderParameterBufferType,
                               sizeof(packedheader_param_buffer), 1, &packedheader_param_buffer,
                               &packedpic_para_bufid);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    va_status = vaCreateBuffer(data->va_dpy,
                               data->context_id,
                               VAEncPackedHeaderDataBufferType,
                               (length_in_bits + 7) / 8, 1, packedpic_buffer,
                               &packedpic_data_bufid);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    render_id[0] = packedpic_para_bufid;
    render_id[1] = packedpic_data_bufid;
    va_status = vaRenderPicture(data->va_dpy, data->context_id, render_id, 2);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    free(packedpic_buffer);

    if (packedpic_para_bufid != VA_INVALID_ID) {
        vaDestroyBuffer(data->va_dpy, packedpic_para_bufid);
        packedpic_para_bufid = VA_INVALID_ID;
    }
    if (packedpic_data_bufid != VA_INVALID_ID) {
        vaDestroyBuffer(data->va_dpy, packedpic_data_bufid);
        packedpic_para_bufid = VA_INVALID_ID;
    }

    return 0;
}

static void render_packedslice(struct vaapi_hevc_encodec *data)
{
    VAEncPackedHeaderParameterBuffer packedheader_param_buffer;
    VABufferID packedslice_para_bufid =  VA_INVALID_ID;
    VABufferID packedslice_data_bufid =  VA_INVALID_ID;
    VABufferID render_id[2] = {VA_INVALID_ID};
    unsigned int length_in_bits;
    unsigned char *packedslice_buffer = NULL;
    VAStatus va_status;

    length_in_bits = build_packed_slice_buffer(data, &packedslice_buffer);
    packedheader_param_buffer.type = VAEncPackedHeaderSlice;
    packedheader_param_buffer.bit_length = length_in_bits;
    packedheader_param_buffer.has_emulation_bytes = 0;

    va_status = vaCreateBuffer(data->va_dpy,
                               data->context_id,
                               VAEncPackedHeaderParameterBufferType,
                               sizeof(packedheader_param_buffer), 1, &packedheader_param_buffer,
                               &packedslice_para_bufid);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    va_status = vaCreateBuffer(data->va_dpy,
                               data->context_id,
                               VAEncPackedHeaderDataBufferType,
                               (length_in_bits + 7) / 8, 1, packedslice_buffer,
                               &packedslice_data_bufid);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");

    render_id[0] = packedslice_para_bufid;
    render_id[1] = packedslice_data_bufid;
    va_status = vaRenderPicture(data->va_dpy, data->context_id, render_id, 2);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    free(packedslice_buffer);

    if (packedslice_para_bufid != VA_INVALID_ID) {
        vaDestroyBuffer(data->va_dpy, packedslice_para_bufid);
        packedslice_para_bufid = VA_INVALID_ID;
    }
    if (packedslice_data_bufid != VA_INVALID_ID) {
        vaDestroyBuffer(data->va_dpy, packedslice_data_bufid);
        packedslice_para_bufid = VA_INVALID_ID;
    }
}

static int render_slice(struct vaapi_hevc_encodec *data)
{
    VABufferID slice_param_buf = VA_INVALID_ID;
    VAStatus va_status;
    memset(&data->slice_param, 0x00, sizeof(VAEncSliceParameterBufferHEVC));

    update_RefPicList(data);

    data->slice_param.slice_segment_address = 0;
    data->slice_param.num_ctu_in_slice = data->ssh.picture_width_in_ctus * data->ssh.picture_height_in_ctus;
    data->slice_param.slice_type = data->ssh.slice_type;
    data->slice_param.slice_pic_parameter_set_id = data->ssh.slice_pic_parameter_set_id; // right???

    data->slice_param.num_ref_idx_l0_active_minus1 = data->ssh.num_ref_idx_l0_active_minus1;
    data->slice_param.num_ref_idx_l1_active_minus1 = data->ssh.num_ref_idx_l1_active_minus1;
    memset(data->slice_param.ref_pic_list0, 0xff, sizeof(data->slice_param.ref_pic_list0));
    memset(data->slice_param.ref_pic_list1, 0xff, sizeof(data->slice_param.ref_pic_list1));

    if (data->current_frame_type == FRAME_P) {
        memcpy(data->slice_param.ref_pic_list0, RefPicList0_P, sizeof(VAPictureHEVC));
        if (p2b) {
            memcpy(data->slice_param.ref_pic_list1, RefPicList0_P, sizeof(VAPictureHEVC));
        }
    } else if (data->current_frame_type == FRAME_B) {
        memcpy(data->slice_param.ref_pic_list0, RefPicList0_B, sizeof(VAPictureHEVC));
        memcpy(data->slice_param.ref_pic_list1, RefPicList1_B, sizeof(VAPictureHEVC));
    }

    data->slice_param.luma_log2_weight_denom = 0;
    data->slice_param.delta_chroma_log2_weight_denom = 0;

    data->slice_param.max_num_merge_cand = 5 - data->ssh.five_minus_max_num_merge_cand;

    data->slice_param.slice_qp_delta = data->ssh.slice_qp_delta;
    data->slice_param.slice_cb_qp_offset = 0;
    data->slice_param.slice_cr_qp_offset = 0;
    data->slice_param.slice_beta_offset_div2 = data->ssh.beta_offset_div2;
    data->slice_param.slice_tc_offset_div2 = data->ssh.tc_offset_div2;

    data->slice_param.slice_fields.bits.dependent_slice_segment_flag = 0;
    data->slice_param.slice_fields.bits.colour_plane_id = data->ssh.colour_plane_id;
    data->slice_param.slice_fields.bits.slice_temporal_mvp_enabled_flag = data->ssh.slice_temporal_mvp_enabled_flag;
    data->slice_param.slice_fields.bits.slice_sao_luma_flag = data->ssh.slice_sao_luma_flag;
    data->slice_param.slice_fields.bits.slice_sao_chroma_flag = data->ssh.slice_sao_luma_flag;
    data->slice_param.slice_fields.bits.num_ref_idx_active_override_flag = data->ssh.num_ref_idx_active_override_flag;
    data->slice_param.slice_fields.bits.mvd_l1_zero_flag = 0;
    data->slice_param.slice_fields.bits.cabac_init_flag = 0;
    data->slice_param.slice_fields.bits.slice_deblocking_filter_disabled_flag = data->ssh.disable_deblocking_filter_flag;
    data->slice_param.slice_fields.bits.slice_loop_filter_across_slices_enabled_flag = data->ssh.slice_loop_filter_across_slices_enabled_flag;
    data->slice_param.slice_fields.bits.collocated_from_l0_flag = data->ssh.collocated_from_l0_flag;

    if (hevc_packedheader &&
        data->config_attrib[enc_packed_header_idx].value & VA_ENC_PACKED_HEADER_SLICE)
        render_packedslice(data);

    va_status = vaCreateBuffer(data->va_dpy, data->context_id, VAEncSliceParameterBufferType,
                               sizeof(data->slice_param), 1, &data->slice_param, &slice_param_buf);
    CHECK_VASTATUS(va_status, "vaCreateBuffer");;

    va_status = vaRenderPicture(data->va_dpy, data->context_id, &slice_param_buf, 1);
    CHECK_VASTATUS(va_status, "vaRenderPicture");

    if (slice_param_buf != VA_INVALID_ID) {
        vaDestroyBuffer(data->va_dpy, slice_param_buf);
        slice_param_buf = VA_INVALID_ID;
    }

    return 0;
}


static int send_pkt_data(struct vaapi_hevc_encodec *data)
{
    VACodedBufferSegment *buf_list = NULL;
    VAStatus va_status;
    unsigned int coded_size = 0;

    va_status = vaSyncSurface(data->va_dpy, data->src_surface[data->current_pkt_idx % SURFACE_NUM]);
    CHECK_VASTATUS(va_status, "vaSyncSurface");

    va_status = vaMapBuffer(data->va_dpy, data->pkt_buf[data->current_pkt_idx % SURFACE_NUM], (void **)(&buf_list));
    CHECK_VASTATUS(va_status, "vaMapBuffer");
    // while (buf_list != NULL) {
    //     coded_size += fwrite(buf_list->buf, 1, buf_list->size, coded_fp);
    //     buf_list = (VACodedBufferSegment *) buf_list->next;

    //     frame_size += coded_size;
    // }
    vaUnmapBuffer(data->va_dpy, data->pkt_buf[data->current_pkt_idx % SURFACE_NUM]);

    printf("\n      "); /* return back to startpoint */
    switch (data->current_frame_encoding % 4) {
    case 0:
        printf("|");
        break;
    case 1:
        printf("/");
        break;
    case 2:
        printf("-");
        break;
    case 3:
        printf("\\");
        break;
    }
    printf("%08lld", data->current_frame_encoding);
    printf("(%06d bytes coded)\n", coded_size);

    return 0;
}


// static int import_buffer(VADisplay va_dpy, VASurfaceID surface_id,
//                               int src_fourcc, int src_width, int src_height,
//                               unsigned char *src_Y, unsigned char *src_U, unsigned char *src_V)
// {
//     VAImage surface_image;
//     unsigned char *surface_p = NULL, *Y_start = NULL, *U_start = NULL;
//     int Y_pitch = 0, U_pitch = 0, row;
//     VAStatus va_status;

//     va_status = vaDeriveImage(va_dpy, surface_id, &surface_image);
//     CHECK_VASTATUS(va_status, "vaDeriveImage");

//     vaMapBuffer(va_dpy, surface_image.buf, (void **)&surface_p);
//     assert(VA_STATUS_SUCCESS == va_status);

//     Y_start = surface_p;
//     Y_pitch = surface_image.pitches[0];
//     switch (surface_image.format.fourcc) {
//     case VA_FOURCC_NV12:
//         U_start = (unsigned char *)surface_p + surface_image.offsets[1];
//         U_pitch = surface_image.pitches[1];
//         break;
//     case VA_FOURCC_IYUV:
//         U_start = (unsigned char *)surface_p + surface_image.offsets[1];
//         U_pitch = surface_image.pitches[1];
//         break;
//     case VA_FOURCC_YV12:
//         U_start = (unsigned char *)surface_p + surface_image.offsets[2];
//         U_pitch = surface_image.pitches[2];
//         break;
//     case VA_FOURCC_YUY2:
//         U_start = surface_p + 1;
//         U_pitch = surface_image.pitches[0];
//         break;
//     default:
//         assert(0);
//     }

//     /* copy Y plane */
//     for (row = 0; row < src_height; row++) {
//         unsigned char *Y_row = Y_start + row * Y_pitch;
//         memcpy(Y_row, src_Y + row * src_width, src_width);
//     }

//     for (row = 0; row < src_height / 2; row++) {
//         unsigned char *U_row = U_start + row * U_pitch;
//         unsigned char *u_ptr = NULL, *v_ptr = NULL;
//         int j;
//         switch (surface_image.format.fourcc) {
//         case VA_FOURCC_NV12:
//             if (src_fourcc == VA_FOURCC_NV12) {
//                 memcpy(U_row, src_U + row * src_width, src_width);
//                 break;
//             } else if (src_fourcc == VA_FOURCC_IYUV) {
//                 u_ptr = src_U + row * (src_width / 2);
//                 v_ptr = src_V + row * (src_width / 2);
//             } else if (src_fourcc == VA_FOURCC_YV12) {
//                 v_ptr = src_U + row * (src_width / 2);
//                 u_ptr = src_V + row * (src_width / 2);
//             }
//             if ((src_fourcc == VA_FOURCC_IYUV) ||
//                 (src_fourcc == VA_FOURCC_YV12)) {
//                 for (j = 0; j < src_width / 2; j++) {
//                     U_row[2 * j] = u_ptr[j];
//                     U_row[2 * j + 1] = v_ptr[j];
//                 }
//             }
//             break;
//         case VA_FOURCC_IYUV:
//         case VA_FOURCC_YV12:
//         case VA_FOURCC_YUY2:
//         default:
//             printf("unsupported fourcc in load_surface_yuv\n");
//             assert(0);
//         }
//     }

//     vaUnmapBuffer(va_dpy, surface_image.buf);

//     vaDestroyImage(va_dpy, surface_image.image_id);

//     return 0;
// }

static int encode_frames(struct vaapi_hevc_encodec *data, struct common_buffer *buffer)
{
    VAStatus va_status;

    for (data->current_frame_encoding = 0; data->current_frame_encoding < frame_count; data->current_frame_encoding++) {
        encoding2display_order(data->current_frame_encoding, intra_period, intra_idr_period, ip_period,
                               &data->current_pkt_idx, &data->current_frame_type);
        if (data->current_frame_type == FRAME_IDR) {
            data->numShortTerm = 0;
            current_frame_num = 0;
            data->current_IDR_display = data->current_pkt_idx;
        }

        va_status = vaBeginPicture(data->va_dpy, data->context_id, data->src_surface[current_slot]);
        CHECK_VASTATUS(va_status, "vaBeginPicture");
        fill_vps_header(&data->vps);
        fill_sps_header(data, 0);
        fill_pps_header(&data->pps, 0, 0);
        if (data->current_frame_type == FRAME_IDR) {
            render_sequence(data);
            render_packedvideo(data);
            render_packedsequence(data);
        }
        render_packedpicture(data);
        render_picture(data);
        fill_slice_header(data);
        render_slice(data);

        va_status = vaEndPicture(data->va_dpy, data->context_id);
        CHECK_VASTATUS(va_status, "vaEndPicture");;

        send_pkt_data(data);

        update_ReferenceFrames(data);
    }

    return 0;
}


static void release_encode(struct vaapi_hevc_encodec *data)
{
    int i;

    vaDestroySurfaces(data->va_dpy, &data->src_surface[0], SURFACE_NUM);
    vaDestroySurfaces(data->va_dpy, &data->ref_surface[0], SURFACE_NUM);

    for (i = 0; i < SURFACE_NUM; i++)
        vaDestroyBuffer(data->va_dpy, data->pkt_buf[i]);

    vaDestroyContext(data->va_dpy, data->context_id);
    vaDestroyConfig(data->va_dpy, data->config_id);
}

static void deinit_va(struct vaapi_hevc_encodec *data)
{
    vaTerminate(data->va_dpy);
    va_close_display(data->va_dpy);
}

static int vaapi_encodec_init(struct module_data *encodec_dev)
{
    struct vaapi_hevc_encodec *priv;

    priv = calloc(1, sizeof(*priv));
    if(!priv)
    {
        func_error("calloc fail, check free memery.");
        goto FAIL1;
    }
    
    priv->hevc_profile = ~0;
    priv->entryPoint = VAEntrypointEncSlice;

    if(init_va(priv) != 0)
        goto FAIL2;

    if(setup_encode(priv) != 0)
        goto FAIL3;

    encodec_dev->priv = priv;

    return 0;

FAIL3:
    deinit_va(priv);
FAIL2:
    free(priv);
FAIL1:
    return -1;
}

// static int vaapi_encodec_set_params(struct module_data *encodec_dev)
// {

// }

static int vaapi_encodec_push_fb(struct module_data *encodec_dev, struct common_buffer *buffer)
{
    return encode_frames(encodec_dev->priv, buffer);
}

static struct common_buffer *vaapi_encodec_get_package(struct module_data *encodec_dev)
{
    return 0;
}

static int vaapi_encodec_release(struct module_data *encodec_dev)
{
    release_encode(encodec_dev->priv);
    deinit_va(encodec_dev->priv);
    return 0;
}

struct encodec_ops vaapi_hevc_encodec_dev = 
{
    .name               = "vaapi_hevc_encodec_dev",
    .init               = vaapi_encodec_init,
    .push_fb            = vaapi_encodec_push_fb,
    .get_package        = vaapi_encodec_get_package,
    .release            = vaapi_encodec_release
};

REGISTE_ENCODEC_DEV(vaapi_hevc_encodec_dev, DEVICE_PRIO_HEIGHT);