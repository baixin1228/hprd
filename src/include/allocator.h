struct frame_buffer {
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint32_t handle;
	uint32_t size;
	uint8_t *vaddr;
};

struct frame_buffer *create_framebuffer(uint32_t width, uint32_t height, uint16_t bpp);
void release_framebuffer(struct frame_buffer *fb);
