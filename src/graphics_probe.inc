/* Opt-in read-only diagnostics for actual native game draws. Included after
 * graphics resource/shader declarations, before draw_vertices_data. This does
 * not alter draw inputs or substitute pixels. See docs/GRAPHICS-PROBE.md. */
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>

#define PROBE_DRAW_LIMIT 16u
#define PROBE_IMAGE_LIMIT 8u
#define PROBE_EDGE_LIMIT 4096u

static struct {
    int initialized;
    uint32_t frame;
    unsigned draws, active, images;
    GLuint query;
    char directory[PATH_MAX];
} s_draw_probe;

static void probe_configure(void)
{
    if (s_draw_probe.initialized) return;
    s_draw_probe.initialized = 1;
    const char *frame = getenv("WRATH_TRACE_FRAME");
    if (!frame || !*frame) return;
    char *end;
    errno = 0;
    unsigned long long number = strtoull(frame, &end, 10);
    if (errno || frame[0] < '0' || frame[0] > '9' || *end ||
        number == 0 || number > UINT32_MAX) {
        fprintf(stderr, "[graphics probe] invalid WRATH_TRACE_FRAME; disabled\n");
        return;
    }
    s_draw_probe.frame = (uint32_t)number;
    const char *directory = getenv("WRATH_TRACE_DIR");
    if (directory && *directory) {
        struct stat info;
        if (*directory != '/' || strlen(directory) >= sizeof(s_draw_probe.directory) ||
            stat(directory, &info) || !S_ISDIR(info.st_mode))
            fprintf(stderr, "[graphics probe] WRATH_TRACE_DIR must be an existing absolute directory; BMPs disabled\n");
        else memcpy(s_draw_probe.directory, directory, strlen(directory) + 1);
    }
    fprintf(stderr, "[graphics probe] frame %u, first %u draws, up to %u BMPs%s\n",
            s_draw_probe.frame, PROBE_DRAW_LIMIT, PROBE_IMAGE_LIMIT,
            s_draw_probe.directory[0] ? " enabled" : " disabled");
}

static unsigned probe_begin(void)
{
    probe_configure();
    if (!s_draw_probe.frame ||
        (uint64_t)read32(GUEST_SWAP_COUNT) + 1 != s_draw_probe.frame ||
        s_draw_probe.draws >= PROBE_DRAW_LIMIT || s_draw_probe.active) return 0;
    unsigned token = ++s_draw_probe.draws;
    s_draw_probe.active = token;
    s_draw_probe.query = 0;
    GLint samples_query = 0, any_query = 0, conservative_query = 0;
    glGetQueryiv(GL_SAMPLES_PASSED, GL_CURRENT_QUERY, &samples_query);
    glGetQueryiv(GL_ANY_SAMPLES_PASSED, GL_CURRENT_QUERY, &any_query);
    if (epoxy_gl_version() >= 43 || epoxy_has_gl_extension("GL_ARB_ES3_compatibility"))
        glGetQueryiv(GL_ANY_SAMPLES_PASSED_CONSERVATIVE, GL_CURRENT_QUERY, &conservative_query);
    if (samples_query || any_query || conservative_query) {
        fprintf(stderr, "[graphics probe] frame %u draw %u: sample query unavailable (active %d/%d/%d)\n",
                s_draw_probe.frame, token, samples_query, any_query, conservative_query);
    } else {
        glGenQueries(1, &s_draw_probe.query);
        if (s_draw_probe.query) glBeginQuery(GL_SAMPLES_PASSED, s_draw_probe.query);
    }
    return token;
}

static void probe_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}
static void probe_u32(uint8_t *p, uint32_t v)
{
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(v >> (8 * i));
}

/* GL bottom-up rows are already the order required by positive-height BMP.
 * Write RGB evidence without changing the readback buffer's alpha or channels. */
static void probe_save_bmp(unsigned token, GLint fbo, unsigned width, unsigned height,
                           const uint8_t *rgba)
{
    if (!s_draw_probe.directory[0] || s_draw_probe.images >= PROBE_IMAGE_LIMIT) return;
    ++s_draw_probe.images; /* Bound attempts too, including disk failures. */
    char path[PATH_MAX];
    int n = snprintf(path, sizeof(path), "%s/frame-%u-draw-%02u-fbo-%d.bmp",
                     s_draw_probe.directory, s_draw_probe.frame, token, fbo);
    if (n < 0 || (size_t)n >= sizeof(path)) {
        fprintf(stderr, "[graphics probe] BMP path too long\n"); return;
    }
    unsigned pitch = (width * 3 + 3) & ~3u;
    uint8_t *row = calloc(1, pitch);
    if (!row) { fprintf(stderr, "[graphics probe] BMP row allocation failed\n"); return; }
    FILE *file = fopen(path, "wb");
    if (!file) {
        fprintf(stderr, "[graphics probe] cannot write %s: %s\n", path, strerror(errno));
        free(row); return;
    }
    uint8_t header[54] = {0};
    header[0] = 'B'; header[1] = 'M';
    probe_u32(header + 2, 54 + pitch * height); probe_u32(header + 10, 54);
    probe_u32(header + 14, 40); probe_u32(header + 18, width);
    probe_u32(header + 22, height); probe_u16(header + 26, 1);
    probe_u16(header + 28, 24); probe_u32(header + 34, pitch * height);
    int ok = fwrite(header, 1, sizeof(header), file) == sizeof(header);
    for (unsigned y = 0; ok && y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            const uint8_t *pixel = rgba + ((size_t)y * width + x) * 4;
            row[x * 3] = pixel[2]; row[x * 3 + 1] = pixel[1]; row[x * 3 + 2] = pixel[0];
        }
        ok = fwrite(row, 1, pitch, file) == pitch;
    }
    if (fclose(file)) ok = 0;
    free(row);
    fprintf(stderr, "[graphics probe] %s BMP %s\n", ok ? "saved actual" : "failed writing", path);
}

static void probe_target(unsigned token, GLint fbo, GLint draw_buffer, GLint samples)
{
    unsigned width = s_target_width, height = s_target_height;
    if (!width || !height || width > PROBE_EDGE_LIMIT || height > PROBE_EDGE_LIMIT) {
        fprintf(stderr, "[graphics probe] target readback skipped: dimensions %ux%u exceed bound\n", width, height);
        return;
    }
    /* glReadPixels from a non-default multisample framebuffer is invalid;
     * do not add a resolve blit that could itself affect the draw probe. */
    if (fbo && samples > 0) {
        fprintf(stderr, "[graphics probe] target readback skipped: multisample FBO (%d samples)\n", samples);
        return;
    }
    size_t pixels = (size_t)width * height;
    void *buffer = malloc(pixels * 4); /* Reused for RGBA8 and depth32f, max 64 MiB. */
    if (!buffer) { fprintf(stderr, "[graphics probe] readback allocation failed\n"); return; }
    GLint old_read_fbo, old_read_buffer, target_read_buffer, old_pack_buffer;
    const GLenum pack_names[] = {GL_PACK_ALIGNMENT, GL_PACK_ROW_LENGTH,
        GL_PACK_SKIP_ROWS, GL_PACK_SKIP_PIXELS, GL_PACK_SWAP_BYTES,
        GL_PACK_LSB_FIRST, GL_PACK_IMAGE_HEIGHT, GL_PACK_SKIP_IMAGES};
    const GLint pack_values[] = {1, 0, 0, 0, GL_FALSE, GL_FALSE, 0, 0};
    GLint old_pack[sizeof(pack_names) / sizeof(pack_names[0])];
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_read_fbo);
    glGetIntegerv(GL_READ_BUFFER, &old_read_buffer);
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &old_pack_buffer);
    for (unsigned i = 0; i < sizeof(pack_names) / sizeof(pack_names[0]); ++i) {
        glGetIntegerv(pack_names[i], &old_pack[i]); glPixelStorei(pack_names[i], pack_values[i]);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)fbo);
    glGetIntegerv(GL_READ_BUFFER, &target_read_buffer);
    GLenum status = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "[graphics probe] target readback skipped: framebuffer status 0x%X\n", status);
    } else {
        GLenum color_buffer = (GLenum)draw_buffer;
        if (!fbo && color_buffer == GL_FRONT_AND_BACK) color_buffer = GL_BACK;
        GLenum color_attachment = fbo ? color_buffer :
            (color_buffer == GL_FRONT || color_buffer == GL_FRONT_LEFT) ? GL_FRONT_LEFT :
            color_buffer == GL_FRONT_RIGHT ? GL_FRONT_RIGHT :
            color_buffer == GL_BACK_RIGHT ? GL_BACK_RIGHT : GL_BACK_LEFT;
        GLint component_type = 0;
        if (color_buffer != GL_NONE)
            glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, color_attachment,
                                                  GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &component_type);
        if (color_buffer == GL_NONE) {
            fprintf(stderr, "[graphics probe] target RGB unavailable: draw buffer NONE\n");
        } else if (component_type != GL_UNSIGNED_NORMALIZED && component_type != GL_SIGNED_NORMALIZED &&
                   component_type != GL_FLOAT) {
            fprintf(stderr, "[graphics probe] target RGB unavailable: attachment component type 0x%X\n", component_type);
        } else {
            glReadBuffer(color_buffer);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
            const uint8_t *rgba = buffer;
            size_t nonblack = 0;
            for (size_t i = 0; i < pixels; ++i)
                nonblack += (rgba[i * 4] | rgba[i * 4 + 1] | rgba[i * 4 + 2]) != 0;
            const uint8_t *center = rgba + ((size_t)(height / 2) * width + width / 2) * 4;
            fprintf(stderr, "[graphics probe] target %ux%u read=0x%X RGB nonblack=%zu/%zu center RGBA=%u,%u,%u,%u\n",
                    width, height, color_buffer, nonblack, pixels, center[0], center[1], center[2], center[3]);
            probe_save_bmp(token, fbo, width, height, rgba);
        }
        GLenum attachment = fbo ? GL_DEPTH_ATTACHMENT : GL_DEPTH;
        GLint type = GL_NONE, depth_bits = 0;
        glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, attachment,
                                              GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
        if (type != GL_NONE)
            glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, attachment,
                                                  GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &depth_bits);
        if (depth_bits > 0) {
            glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, buffer);
            const float *depth = buffer;
            float min = INFINITY, max = -INFINITY;
            size_t finite = 0;
            for (size_t i = 0; i < pixels; ++i) if (isfinite(depth[i])) {
                if (depth[i] < min) min = depth[i];
                if (depth[i] > max) max = depth[i];
                ++finite;
            }
            fprintf(stderr, "[graphics probe] depth bits=%d finite=%zu/%zu min=%.9g max=%.9g center=%.9g\n",
                    depth_bits, finite, pixels, min, max, depth[(size_t)(height / 2) * width + width / 2]);
        } else fprintf(stderr, "[graphics probe] depth unavailable: no depth attachment\n");
    }
    /* Read-buffer selection belongs to each FBO: restore both objects when
     * the previous read FBO differed from the inspected draw FBO. */
    glReadBuffer((GLenum)target_read_buffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)old_read_fbo);
    glReadBuffer((GLenum)old_read_buffer);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, (GLuint)old_pack_buffer);
    for (unsigned i = 0; i < sizeof(pack_names) / sizeof(pack_names[0]); ++i)
        glPixelStorei(pack_names[i], old_pack[i]);
    free(buffer);
}

static void probe_end(unsigned token, HRESULT result)
{
    if (!token || token != s_draw_probe.active) return;
    if (s_draw_probe.query) {
        GLuint64 samples;
        glEndQuery(GL_SAMPLES_PASSED);
        glGetQueryObjectui64v(s_draw_probe.query, GL_QUERY_RESULT, &samples);
        glDeleteQueries(1, &s_draw_probe.query); s_draw_probe.query = 0;
        fprintf(stderr, "[graphics probe] frame %u draw %u HRESULT=0x%08X samples=%llu\n",
                s_draw_probe.frame, token, (unsigned)result, (unsigned long long)samples);
    } else fprintf(stderr, "[graphics probe] frame %u draw %u HRESULT=0x%08X samples=unavailable\n",
                   s_draw_probe.frame, token, (unsigned)result);
    GLint fbo, program, viewport[4], depth_func, blend[6], front, cull, draw_buffer, samples;
    GLboolean color[4], depth_write;
    GLdouble depth_range[2];
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fbo); glGetIntegerv(GL_CURRENT_PROGRAM, &program);
    glGetIntegerv(GL_VIEWPORT, viewport); glGetIntegerv(GL_DEPTH_FUNC, &depth_func);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write); glGetDoublev(GL_DEPTH_RANGE, depth_range);
    const GLenum blend_names[] = {GL_BLEND_SRC_RGB, GL_BLEND_DST_RGB, GL_BLEND_SRC_ALPHA,
        GL_BLEND_DST_ALPHA, GL_BLEND_EQUATION_RGB, GL_BLEND_EQUATION_ALPHA};
    for (unsigned i = 0; i < 6; ++i) glGetIntegerv(blend_names[i], &blend[i]);
    glGetIntegerv(GL_FRONT_FACE, &front); glGetIntegerv(GL_CULL_FACE_MODE, &cull);
    glGetBooleanv(GL_COLOR_WRITEMASK, color); glGetIntegerv(GL_DRAW_BUFFER0, &draw_buffer);
    glGetIntegerv(GL_SAMPLES, &samples);
    fprintf(stderr, "[graphics probe] FBO=%d buffer=0x%X program=%d viewport=%d,%d,%d,%d multisamples=%d colorwrite=%u%u%u%u\n",
            fbo, draw_buffer, program, viewport[0], viewport[1], viewport[2], viewport[3], samples,
            color[0], color[1], color[2], color[3]);
    fprintf(stderr, "[graphics probe] depth enable=%u write=%u func=0x%X range=%.9g,%.9g blend enable=%u srcRGB/dstRGB/srcA/dstA=0x%X/0x%X/0x%X/0x%X equationRGB/A=0x%X/0x%X cull enable=%u front=0x%X mode=0x%X\n",
            glIsEnabled(GL_DEPTH_TEST), depth_write, depth_func, depth_range[0], depth_range[1],
            glIsEnabled(GL_BLEND), blend[0], blend[1], blend[2], blend[3], blend[4], blend[5],
            glIsEnabled(GL_CULL_FACE), front, cull);
    GLint scissor[4]; glGetIntegerv(GL_SCISSOR_BOX, scissor);
    fprintf(stderr, "[graphics probe] scissor enable=%u box=%d,%d,%d,%d stencil enable=%u rasterizer discard=%u\n",
            glIsEnabled(GL_SCISSOR_TEST), scissor[0], scissor[1], scissor[2], scissor[3],
            glIsEnabled(GL_STENCIL_TEST), glIsEnabled(GL_RASTERIZER_DISCARD));
    probe_target(token, fbo, draw_buffer, samples);
    s_draw_probe.active = 0;
}
