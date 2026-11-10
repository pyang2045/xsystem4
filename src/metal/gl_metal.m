/* The OpenGL ES subset of include/gfx/gl_metal.h, on Metal.
 *
 * GL objects are kept in tables by name. Rendering is deferred into one command
 * buffer, committed at present and whenever the CPU needs a result (glReadPixels,
 * glFinish). Everything the GL orders implicitly is put in that command buffer in
 * call order: draws and clears (render passes), texture uploads and copies (blit
 * passes); so a texture re-specified after a draw that sampled it does not change
 * what that draw sees.
 *
 * Memory layout matches GL: row 0 of a texture is the first row uploaded and the
 * row that GL's window y = 0 renders to. Metal's clip space has +y at row 0, so the
 * vertex shaders multiply gl_Position.y by -1 for textures (xs4_flip, see
 * shaders/metal/glsl2msl.py) and +1 for the window, where both put +y at the top.
 *
 * Shaders are MSL made from the GLSL by shaders/metal/glsl2msl.py; their header
 * comments give the uniform layout and the texture, attribute and varying slots.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://gnu.org/licenses/>.
 */

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>
#include <string.h>

#include "system4.h"
#include "gfx/gl_metal.h"

#define MAX_UNITS 16
#define MAX_ATTRIBS 16
#define VERTEX_BUFFER_INDEX 16
#define FLIP_BUFFER_INDEX 30
#define MAX_TEXTURE_SIZE 16384

static void unsupported(const char *func)
{
	ERROR("%s: not supported by the Metal renderer (no 3D)", func);
}
#define UNSUPPORTED() unsupported(__func__)

/* --- objects --- */

@interface XTex : NSObject {
@public
	id<MTLTexture> tex;  // nil while 0x0 or unspecified
	int w, h;
	GLenum format;       // GL_RGBA, GL_RGB (alpha kept at 1) or GL_R8
	GLenum min_filter, mag_filter, wrap_s, wrap_t;
}
@end
@implementation XTex
@end

@interface XFbo : NSObject {
@public
	GLuint color;
}
@end
@implementation XFbo
@end

@interface XBuf : NSObject {
@public
	NSMutableData *data;
}
@end
@implementation XBuf
@end

// One uniform, texture or varying slot from a shader header.
@interface XDecl : NSObject {
@public
	NSString *name;
	int offset, count, stride, comps, cols;  // uniforms; comps per column
	int index, sampler;                      // textures; varyings and attributes use index
}
@end
@implementation XDecl
@end

@interface XShader : NSObject {
@public
	GLenum type;
	NSString *source;
	id<MTLFunction> fn;
	NSString *log;
	int ubo, ubo_size;  // ubo = -1: none
	NSMutableArray<XDecl *> *uniforms, *textures, *attrs, *ins, *outs;
}
@end
@implementation XShader
@end

// A program uniform: the value lives in each stage's buffer that has it.
@interface XUniform : NSObject {
@public
	NSString *name;
	int count, comps, cols;
	int voff, vstride, foff, fstride;  // -1: not in that stage
	bool sampler;
	int unit;
	int vtex, vsmp, ftex, fsmp;
}
@end
@implementation XUniform
@end

@interface XProgram : NSObject {
@public
	XShader *vs, *fs;
	bool linked;
	NSString *log;
	NSMutableArray<XUniform *> *uniforms;
	NSMutableData *vbuf, *fbuf;
	NSMutableDictionary<NSString *, id<MTLRenderPipelineState>> *pipelines;
}
@end
@implementation XProgram
@end

/* --- state --- */

static id<MTLDevice> device;
static id<MTLCommandQueue> queue;
static CAMetalLayer *layer;
static SDL_MetalView metal_view;
static SDL_Window *window;
static id<MTLCommandBuffer> cmdbuf;
static id<MTLRenderCommandEncoder> encoder;
static id<MTLTexture> encoder_target;
static id<CAMetalDrawable> drawable;
static id<MTLTexture> dummy_texture;
static NSMutableDictionary<NSNumber *, id> *objects;
static NSMutableDictionary<NSNumber *, id<MTLSamplerState>> *samplers;
static NSMutableDictionary<NSString *, id<MTLFunction>> *functions;
static GLuint next_name = 1;
static bool vsync = true;

static GLuint draw_fb, read_fb;
static struct { GLint x, y; GLsizei w, h; } viewport;
static float clear_color[4];
static struct {
	bool enabled;
	GLenum src_rgb, dst_rgb, src_alpha, dst_alpha, eq_rgb, eq_alpha;
	float color[4];
} blend = { false, GL_ONE, GL_ZERO, GL_ONE, GL_ZERO, GL_FUNC_ADD, GL_FUNC_ADD, { 0, 0, 0, 0 } };
static int active_unit;
static GLuint units[MAX_UNITS];
static GLuint array_buffer, element_buffer;
static struct {
	bool enabled;
	GLint size;
	GLsizei stride;
	uintptr_t offset;
	GLuint buffer;
} attribs[MAX_ATTRIBS];
static GLuint current_program;
static GLint unpack_alignment = 4;

static GLuint new_name(id obj)
{
	GLuint name = next_name++;
	if (obj)
		objects[@(name)] = obj;
	return name;
}

static id object(GLuint name, Class cls)
{
	if (!name)
		return nil;
	id obj = objects[@(name)];
	return [obj isKindOfClass:cls] ? obj : nil;
}

static XTex *bound_texture(void)
{
	return object(units[active_unit], [XTex class]);
}

/* --- command stream --- */

static id<MTLCommandBuffer> commands(void)
{
	if (!cmdbuf)
		cmdbuf = [queue commandBuffer];
	return cmdbuf;
}

static void end_encoder(void)
{
	if (encoder) {
		[encoder endEncoding];
		encoder = nil;
		encoder_target = nil;
	}
}

static void commit(bool wait)
{
	end_encoder();
	if (!cmdbuf)
		return;
	[cmdbuf commit];
	if (wait)
		[cmdbuf waitUntilCompleted];
	cmdbuf = nil;
}

// The texture behind a framebuffer (the window's drawable for 0); nil if none.
static id<MTLTexture> fb_texture(GLuint fb, XTex **tex_out)
{
	if (tex_out)
		*tex_out = nil;
	if (!fb) {
		if (!drawable) {
			int w, h;
			SDL_GetWindowSizeInPixels(window, &w, &h);
			if (w <= 0 || h <= 0)
				return nil;
			if (layer.drawableSize.width != w || layer.drawableSize.height != h)
				layer.drawableSize = CGSizeMake(w, h);
			drawable = [layer nextDrawable];
		}
		return drawable.texture;
	}
	XFbo *fbo = object(fb, [XFbo class]);
	XTex *t = fbo ? object(fbo->color, [XTex class]) : nil;
	if (tex_out)
		*tex_out = t;
	return t ? t->tex : nil;
}

static id<MTLRenderCommandEncoder> begin_pass(id<MTLTexture> target, bool clear, MTLClearColor color)
{
	end_encoder();
	MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
	pass.colorAttachments[0].texture = target;
	pass.colorAttachments[0].loadAction = clear ? MTLLoadActionClear : MTLLoadActionLoad;
	pass.colorAttachments[0].storeAction = MTLStoreActionStore;
	pass.colorAttachments[0].clearColor = color;
	encoder = [commands() renderCommandEncoderWithDescriptor:pass];
	encoder_target = target;
	return encoder;
}

static void clear_texture(id<MTLTexture> target, bool rgb)
{
	begin_pass(target, true, MTLClearColorMake(0, 0, 0, rgb ? 1 : 0));
	end_encoder();
}

/* --- window --- */

void metal_init(SDL_Window *win)
{
	window = win;
	device = MTLCreateSystemDefaultDevice();
	if (!device)
		ERROR("No Metal device");
	queue = [device newCommandQueue];
	metal_view = SDL_Metal_CreateView(win);
	if (!metal_view)
		ERROR("SDL_Metal_CreateView failed: %s", SDL_GetError());
	layer = (__bridge CAMetalLayer *)SDL_Metal_GetLayer(metal_view);
	layer.device = device;
	layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
	layer.framebufferOnly = YES;
#if TARGET_OS_OSX
	layer.displaySyncEnabled = vsync;
#endif
	objects = [NSMutableDictionary dictionary];
	samplers = [NSMutableDictionary dictionary];
	functions = [NSMutableDictionary dictionary];

	MTLTextureDescriptor *d = [MTLTextureDescriptor
		texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
	dummy_texture = [device newTextureWithDescriptor:d];
	const uint8_t black[4] = { 0, 0, 0, 255 };
	[dummy_texture replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:black bytesPerRow:4];
}

void metal_fini(void)
{
	commit(true);
	drawable = nil;
	if (metal_view)
		SDL_Metal_DestroyView(metal_view);
	metal_view = NULL;
}

void metal_drawable_size(int *w, int *h)
{
	SDL_GetWindowSizeInPixels(window, w, h);
}

void metal_set_vsync(bool enable)
{
	vsync = enable;
#if TARGET_OS_OSX
	if (layer)
		layer.displaySyncEnabled = enable;
#endif
}

bool metal_vsync(void)
{
	return vsync;
}

void metal_present(void)
{
	@autoreleasepool {
		end_encoder();
		if (drawable)
			[commands() presentDrawable:drawable];
		drawable = nil;
		commit(false);
	}
}

/* --- state calls --- */

const GLubyte *glGetString(GLenum name)
{
	static char renderer[256];
	switch (name) {
	case GL_VERSION:
		return (const GLubyte *)"Metal";
	case GL_RENDERER:
		snprintf(renderer, sizeof(renderer), "%s", device.name.UTF8String);
		return (const GLubyte *)renderer;
	}
	return (const GLubyte *)"";
}

void glGetIntegerv(GLenum pname, GLint *data)
{
	switch (pname) {
	case GL_MAX_TEXTURE_SIZE:
		*data = MAX_TEXTURE_SIZE;
		break;
	case GL_VIEWPORT:
		data[0] = viewport.x;
		data[1] = viewport.y;
		data[2] = viewport.w;
		data[3] = viewport.h;
		break;
	case GL_FRAMEBUFFER_BINDING:
		*data = draw_fb;
		break;
	default:
		ERROR("glGetIntegerv: 0x%x", pname);
	}
}

void glEnable(GLenum cap)
{
	if (cap == GL_BLEND)
		blend.enabled = true;
	else
		UNSUPPORTED();
}

void glDisable(GLenum cap)
{
	if (cap == GL_BLEND)
		blend.enabled = false;
	else if (cap != GL_DEPTH_TEST && cap != GL_CULL_FACE && cap != GL_POLYGON_OFFSET_FILL)
		UNSUPPORTED();
}

void glPixelStorei(GLenum pname, GLint param)
{
	if (pname == GL_UNPACK_ALIGNMENT)
		unpack_alignment = param;
}

void glFlush(void)
{
}

void glFinish(void)
{
	@autoreleasepool {
		commit(true);
	}
}

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	viewport.x = x;
	viewport.y = y;
	viewport.w = width;
	viewport.h = height;
}

void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
	clear_color[0] = r;
	clear_color[1] = g;
	clear_color[2] = b;
	clear_color[3] = a;
}

static float clamp01(float f)
{
	return f < 0 ? 0 : f > 1 ? 1 : f;
}

void glClear(GLbitfield mask)
{
	if (!(mask & GL_COLOR_BUFFER_BIT))
		return;
	@autoreleasepool {
		XTex *t;
		id<MTLTexture> target = fb_texture(draw_fb, &t);
		if (!target)
			return;
		bool rgb = t && t->format == GL_RGB;
		begin_pass(target, true, MTLClearColorMake(clamp01(clear_color[0]), clamp01(clear_color[1]),
				clamp01(clear_color[2]), rgb ? 1 : clamp01(clear_color[3])));
	}
}

void glBlendFunc(GLenum sfactor, GLenum dfactor)
{
	glBlendFuncSeparate(sfactor, dfactor, sfactor, dfactor);
}

void glBlendFuncSeparate(GLenum srgb, GLenum drgb, GLenum salpha, GLenum dalpha)
{
	blend.src_rgb = srgb;
	blend.dst_rgb = drgb;
	blend.src_alpha = salpha;
	blend.dst_alpha = dalpha;
}

void glBlendEquationSeparate(GLenum mode_rgb, GLenum mode_alpha)
{
	blend.eq_rgb = mode_rgb;
	blend.eq_alpha = mode_alpha;
}

void glBlendColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
	blend.color[0] = clamp01(r);
	blend.color[1] = clamp01(g);
	blend.color[2] = clamp01(b);
	blend.color[3] = clamp01(a);
}

/* --- textures --- */

void glGenTextures(GLsizei n, GLuint *textures)
{
	for (int i = 0; i < n; i++) {
		XTex *t = [XTex new];
		t->min_filter = GL_NEAREST_MIPMAP_LINEAR;
		t->mag_filter = GL_LINEAR;
		t->wrap_s = t->wrap_t = GL_REPEAT;
		textures[i] = new_name(t);
	}
}

void glDeleteTextures(GLsizei n, const GLuint *textures)
{
	for (int i = 0; i < n; i++) {
		if (!object(textures[i], [XTex class]))
			continue;
		[objects removeObjectForKey:@(textures[i])];
		for (int u = 0; u < MAX_UNITS; u++) {
			if (units[u] == textures[i])
				units[u] = 0;
		}
	}
}

void glActiveTexture(GLenum texture)
{
	int unit = texture - GL_TEXTURE0;
	if (unit < 0 || unit >= MAX_UNITS)
		ERROR("glActiveTexture: unit %d", unit);
	active_unit = unit;
}

void glBindTexture(GLenum target, GLuint texture)
{
	if (target != GL_TEXTURE_2D)
		UNSUPPORTED();
	units[active_unit] = texture;
}

void glTexParameteri(GLenum target, GLenum pname, GLint param)
{
	XTex *t = bound_texture();
	if (target != GL_TEXTURE_2D || !t)
		return;
	switch (pname) {
	case GL_TEXTURE_MIN_FILTER: t->min_filter = param; break;
	case GL_TEXTURE_MAG_FILTER: t->mag_filter = param; break;
	case GL_TEXTURE_WRAP_S: t->wrap_s = param; break;
	case GL_TEXTURE_WRAP_T: t->wrap_t = param; break;
	}
}

static int format_bpp(GLenum format)
{
	switch (format) {
	case GL_RGBA: return 4;
	case GL_RGB: return 3;
	case GL_RED: return 1;
	}
	ERROR("unsupported pixel format 0x%x", format);
}

// (Re)creates t's storage if the size or format changed; new storage is zeroed
// (alpha 1 for GL_RGB), as on ANGLE.
static void specify(XTex *t, GLenum format, int w, int h)
{
	if (t->tex && t->w == w && t->h == h && t->format == format)
		return;
	t->w = w;
	t->h = h;
	t->format = format;
	t->tex = nil;
	if (w <= 0 || h <= 0)
		return;
	if (w > MAX_TEXTURE_SIZE || h > MAX_TEXTURE_SIZE)
		ERROR("texture size %dx%d", w, h);
	MTLTextureDescriptor *d = [MTLTextureDescriptor
		texture2DDescriptorWithPixelFormat:format == GL_R8 ? MTLPixelFormatR8Unorm : MTLPixelFormatRGBA8Unorm
		width:w height:h mipmapped:NO];
	d.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
	d.storageMode = MTLStorageModePrivate;
	t->tex = [device newTextureWithDescriptor:d];
	clear_texture(t->tex, format == GL_RGB);
}

// Encodes an upload of w*h pixels (in GL's format/type and unpack alignment) at (x, y).
static void upload(XTex *t, int x, int y, int w, int h, GLenum format, const uint8_t *pixels)
{
	int bpp = format_bpp(format);
	int dst_bpp = t->format == GL_R8 ? 1 : 4;
	size_t src_pitch = ((size_t)w * bpp + unpack_alignment - 1) / unpack_alignment * unpack_alignment;
	size_t dst_pitch = (size_t)w * dst_bpp;
	id<MTLBuffer> staging = [device newBufferWithLength:dst_pitch * h options:MTLResourceStorageModeShared];
	uint8_t *dst = staging.contents;
	for (int row = 0; row < h; row++) {
		const uint8_t *s = pixels + row * src_pitch;
		uint8_t *d = dst + row * dst_pitch;
		if (bpp == dst_bpp) {
			memcpy(d, s, dst_pitch);
		} else if (bpp == 3 && dst_bpp == 4) {
			for (int i = 0; i < w; i++) {
				d[i*4+0] = s[i*3+0];
				d[i*4+1] = s[i*3+1];
				d[i*4+2] = s[i*3+2];
				d[i*4+3] = 255;
			}
		} else {
			ERROR("upload: %d bytes per pixel into %d", bpp, dst_bpp);
		}
	}
	end_encoder();
	id<MTLBlitCommandEncoder> blit = [commands() blitCommandEncoder];
	[blit copyFromBuffer:staging sourceOffset:0 sourceBytesPerRow:dst_pitch sourceBytesPerImage:dst_pitch * h
		  sourceSize:MTLSizeMake(w, h, 1) toTexture:t->tex destinationSlice:0 destinationLevel:0
		  destinationOrigin:MTLOriginMake(x, y, 0)];
	[blit endEncoding];
}

void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height,
		  GLint border, GLenum format, GLenum type, const void *pixels)
{
	XTex *t = bound_texture();
	if (target != GL_TEXTURE_2D || level != 0 || type != GL_UNSIGNED_BYTE)
		UNSUPPORTED();
	if (!t)
		return;
	GLenum f;
	switch (internalformat) {
	case GL_RGBA: f = GL_RGBA; break;
	case GL_RGB: f = GL_RGB; break;
	case GL_R8: case GL_RED: f = GL_R8; break;
	default: UNSUPPORTED(); return;
	}
	@autoreleasepool {
		specify(t, f, width, height);
		if (pixels && t->tex)
			upload(t, 0, 0, width, height, format, pixels);
	}
}

void glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y,
		      GLsizei width, GLsizei height, GLint border)
{
	XTex *t = bound_texture();
	if (target != GL_TEXTURE_2D || level != 0 || (internalformat != GL_RGB && internalformat != GL_RGBA))
		UNSUPPORTED();
	if (!t)
		return;
	@autoreleasepool {
		XTex *src_tex;
		id<MTLTexture> src = read_fb ? fb_texture(read_fb, &src_tex) : nil;
		if (!src || src.pixelFormat != MTLPixelFormatRGBA8Unorm)
			ERROR("glCopyTexImage2D: no RGBA texture to read");
		specify(t, internalformat, width, height);
		if (!t->tex)
			return;
		end_encoder();
		id<MTLBlitCommandEncoder> blit = [commands() blitCommandEncoder];
		[blit copyFromTexture:src sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(x, y, 0)
			   sourceSize:MTLSizeMake(width, height, 1) toTexture:t->tex destinationSlice:0
			   destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
		[blit endEncoding];
	}
}

void glGenerateMipmap(GLenum target)
{
	UNSUPPORTED();
}

void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels)
{
	if (format != GL_RGBA || type != GL_UNSIGNED_BYTE)
		UNSUPPORTED();
	@autoreleasepool {
		XTex *t;
		id<MTLTexture> src = read_fb ? fb_texture(read_fb, &t) : nil;
		if (!src) {
			WARNING("glReadPixels: no texture to read");
			memset(pixels, 0, (size_t)width * height * 4);
			return;
		}
		bool r8 = src.pixelFormat == MTLPixelFormatR8Unorm;
		size_t pitch = (size_t)width * (r8 ? 1 : 4);
		id<MTLBuffer> buf = [device newBufferWithLength:pitch * height options:MTLResourceStorageModeShared];
		end_encoder();
		id<MTLBlitCommandEncoder> blit = [commands() blitCommandEncoder];
		[blit copyFromTexture:src sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(x, y, 0)
			   sourceSize:MTLSizeMake(width, height, 1) toBuffer:buf destinationOffset:0
		  destinationBytesPerRow:pitch destinationBytesPerImage:pitch * height];
		[blit endEncoding];
		commit(true);
		if (!r8) {
			memcpy(pixels, buf.contents, pitch * height);
			return;
		}
		const uint8_t *s = buf.contents;
		uint8_t *d = pixels;
		for (size_t i = 0; i < (size_t)width * height; i++) {
			d[i*4+0] = s[i];
			d[i*4+1] = 0;
			d[i*4+2] = 0;
			d[i*4+3] = 255;
		}
	}
}

void glReadBuffer(GLenum src)
{
	UNSUPPORTED();
}

/* --- framebuffers --- */

void glGenFramebuffers(GLsizei n, GLuint *framebuffers)
{
	for (int i = 0; i < n; i++)
		framebuffers[i] = new_name([XFbo new]);
}

void glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers)
{
	for (int i = 0; i < n; i++) {
		if (!object(framebuffers[i], [XFbo class]))
			continue;
		[objects removeObjectForKey:@(framebuffers[i])];
		if (draw_fb == framebuffers[i])
			draw_fb = 0;
		if (read_fb == framebuffers[i])
			read_fb = 0;
	}
}

void glBindFramebuffer(GLenum target, GLuint framebuffer)
{
	if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER)
		draw_fb = framebuffer;
	if (target == GL_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER)
		read_fb = framebuffer;
}

void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
	if (attachment != GL_COLOR_ATTACHMENT0 || textarget != GL_TEXTURE_2D || level != 0)
		UNSUPPORTED();
	XFbo *fbo = object(target == GL_READ_FRAMEBUFFER ? read_fb : draw_fb, [XFbo class]);
	if (fbo)
		fbo->color = texture;
}

GLenum glCheckFramebufferStatus(GLenum target)
{
	XFbo *fbo = object(target == GL_READ_FRAMEBUFFER ? read_fb : draw_fb, [XFbo class]);
	if (!fbo)
		return GL_FRAMEBUFFER_COMPLETE;  // the window
	XTex *t = object(fbo->color, [XTex class]);
	return t && t->tex ? GL_FRAMEBUFFER_COMPLETE : 0;
}

void glDrawBuffers(GLsizei n, const GLenum *bufs) { UNSUPPORTED(); }
void glGenRenderbuffers(GLsizei n, GLuint *renderbuffers) { UNSUPPORTED(); }
void glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers) { UNSUPPORTED(); }
void glBindRenderbuffer(GLenum target, GLuint renderbuffer) { UNSUPPORTED(); }
void glRenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) { UNSUPPORTED(); }
void glFramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum rbtarget, GLuint renderbuffer) { UNSUPPORTED(); }

/* --- buffers and vertex arrays --- */

void glGenBuffers(GLsizei n, GLuint *buffers)
{
	for (int i = 0; i < n; i++) {
		XBuf *b = [XBuf new];
		b->data = [NSMutableData data];
		buffers[i] = new_name(b);
	}
}

void glDeleteBuffers(GLsizei n, const GLuint *buffers)
{
	for (int i = 0; i < n; i++) {
		if (object(buffers[i], [XBuf class]))
			[objects removeObjectForKey:@(buffers[i])];
	}
}

void glBindBuffer(GLenum target, GLuint buffer)
{
	if (target == GL_ARRAY_BUFFER)
		array_buffer = buffer;
	else if (target == GL_ELEMENT_ARRAY_BUFFER)
		element_buffer = buffer;
	else
		UNSUPPORTED();
}

void glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
	XBuf *b = object(target == GL_ARRAY_BUFFER ? array_buffer : element_buffer, [XBuf class]);
	if (!b)
		return;
	b->data.length = size;
	if (data)
		memcpy(b->data.mutableBytes, data, size);
}

void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data) { UNSUPPORTED(); }
void glBindBufferBase(GLenum target, GLuint index, GLuint buffer) { UNSUPPORTED(); }

// One vertex array is enough: the 2D renderer sets the attributes before each draw.
void glGenVertexArrays(GLsizei n, GLuint *arrays)
{
	for (int i = 0; i < n; i++)
		arrays[i] = new_name(nil);
}

void glDeleteVertexArrays(GLsizei n, const GLuint *arrays)
{
}

void glBindVertexArray(GLuint array)
{
}

void glEnableVertexAttribArray(GLuint index)
{
	if (index < MAX_ATTRIBS)
		attribs[index].enabled = true;
}

void glDisableVertexAttribArray(GLuint index)
{
	if (index < MAX_ATTRIBS)
		attribs[index].enabled = false;
}

void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride,
			   const void *pointer)
{
	if (index >= MAX_ATTRIBS || type != GL_FLOAT || normalized)
		UNSUPPORTED();
	attribs[index].size = size;
	attribs[index].stride = stride ? stride : size * 4;
	attribs[index].offset = (uintptr_t)pointer;
	attribs[index].buffer = array_buffer;
}

void glVertexAttribIPointer(GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer) { UNSUPPORTED(); }
void glVertexAttrib1f(GLuint index, GLfloat x) { UNSUPPORTED(); }
void glVertexAttrib2f(GLuint index, GLfloat x, GLfloat y) { UNSUPPORTED(); }
void glVertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z) { UNSUPPORTED(); }
void glVertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) { UNSUPPORTED(); }
void glVertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w) { UNSUPPORTED(); }

/* --- shaders and programs --- */

static int type_comps(NSString *type, int *cols)
{
	*cols = 1;
	if ([type isEqualToString:@"float"] || [type isEqualToString:@"int"] || [type isEqualToString:@"uint"])
		return 1;
	if ([type isEqualToString:@"vec2"] || [type isEqualToString:@"ivec2"])
		return 2;
	if ([type isEqualToString:@"vec3"] || [type isEqualToString:@"ivec3"])
		return 3;
	if ([type isEqualToString:@"vec4"] || [type isEqualToString:@"ivec4"])
		return 4;
	if ([type isEqualToString:@"mat3"]) {
		*cols = 3;
		return 3;
	}
	if ([type isEqualToString:@"mat4"]) {
		*cols = 4;
		return 4;
	}
	ERROR("uniform type %s", type.UTF8String);
}

GLuint glCreateShader(GLenum type)
{
	XShader *s = [XShader new];
	s->type = type;
	return new_name(s);
}

void glShaderSource(GLuint shader, GLsizei count, const GLchar *const *string, const GLint *length)
{
	XShader *s = object(shader, [XShader class]);
	NSMutableString *src = [NSMutableString string];
	for (int i = 0; i < count; i++) {
		NSString *part = length && length[i] >= 0
			? [[NSString alloc] initWithBytes:string[i] length:length[i] encoding:NSUTF8StringEncoding]
			: [NSString stringWithUTF8String:string[i]];
		[src appendString:part];
	}
	s->source = src;
}

static void parse_header(XShader *s)
{
	s->ubo = -1;
	s->uniforms = [NSMutableArray array];
	s->textures = [NSMutableArray array];
	s->attrs = [NSMutableArray array];
	s->ins = [NSMutableArray array];
	s->outs = [NSMutableArray array];
	for (NSString *line in [s->source componentsSeparatedByString:@"\n"]) {
		if (![line hasPrefix:@"// @"])
			continue;
		NSArray<NSString *> *f = [[line substringFromIndex:4] componentsSeparatedByString:@" "];
		NSString *kind = f[0];
		XDecl *d = [XDecl new];
		if ([kind isEqualToString:@"ubo"]) {
			s->ubo = f[1].intValue;
			s->ubo_size = f[2].intValue;
			continue;
		} else if ([kind isEqualToString:@"u"]) {
			d->name = f[1];
			d->offset = f[2].intValue;
			d->comps = type_comps(f[3], &d->cols);
			d->count = f[4].intValue;
			d->stride = f[5].intValue;
			[s->uniforms addObject:d];
		} else if ([kind isEqualToString:@"tex"]) {
			d->name = f[1];
			d->index = f[2].intValue;
			d->sampler = f[3].intValue;
			[s->textures addObject:d];
		} else {
			d->name = f[1];
			d->index = f[2].intValue;
			if ([kind isEqualToString:@"attr"])
				[s->attrs addObject:d];
			else if ([kind isEqualToString:@"in"])
				[s->ins addObject:d];
			else if ([kind isEqualToString:@"out"])
				[s->outs addObject:d];
		}
	}
}

void glCompileShader(GLuint shader)
{
	XShader *s = object(shader, [XShader class]);
	if (!s)
		return;
	@autoreleasepool {
		parse_header(s);
		s->fn = functions[s->source];
		if (s->fn)
			return;
		MTLCompileOptions *opt = [MTLCompileOptions new];
		opt.languageVersion = MTLLanguageVersion2_1;
		// Precise math: the results should match ANGLE's to the bit.
		if (@available(macOS 15.0, iOS 18.0, *)) {
			opt.mathMode = MTLMathModeSafe;
		} else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
			opt.fastMathEnabled = NO;
#pragma clang diagnostic pop
		}
		NSError *err = nil;
		id<MTLLibrary> lib = [device newLibraryWithSource:s->source options:opt error:&err];
		s->fn = [lib newFunctionWithName:@"main0"];
		if (!s->fn)
			s->log = err ? err.localizedDescription : @"no main0";
		else
			functions[s->source] = s->fn;
	}
}

void glGetShaderiv(GLuint shader, GLenum pname, GLint *params)
{
	XShader *s = object(shader, [XShader class]);
	if (pname == GL_COMPILE_STATUS)
		*params = s && s->fn ? GL_TRUE : GL_FALSE;
	else if (pname == GL_INFO_LOG_LENGTH)
		*params = s && s->log ? (GLint)strlen(s->log.UTF8String) + 1 : 0;
}

static void copy_log(NSString *log, GLsizei bufsize, GLsizei *length, GLchar *infolog)
{
	const char *str = log ? log.UTF8String : "";
	size_t n = strlen(str);
	if (bufsize <= 0)
		return;
	if (n >= (size_t)bufsize)
		n = bufsize - 1;
	memcpy(infolog, str, n);
	infolog[n] = 0;
	if (length)
		*length = n;
}

void glGetShaderInfoLog(GLuint shader, GLsizei bufsize, GLsizei *length, GLchar *infolog)
{
	XShader *s = object(shader, [XShader class]);
	copy_log(s ? s->log : nil, bufsize, length, infolog);
}

GLuint glCreateProgram(void)
{
	XProgram *p = [XProgram new];
	p->pipelines = [NSMutableDictionary dictionary];
	return new_name(p);
}

void glAttachShader(GLuint program, GLuint shader)
{
	XProgram *p = object(program, [XProgram class]);
	XShader *s = object(shader, [XShader class]);
	if (!p || !s)
		return;
	if (s->type == GL_VERTEX_SHADER)
		p->vs = s;
	else
		p->fs = s;
}

void glBindAttribLocation(GLuint program, GLuint index, const GLchar *name)
{
	UNSUPPORTED();
}

static XUniform *program_uniform(XProgram *p, NSString *name)
{
	for (XUniform *u in p->uniforms) {
		if ([u->name isEqualToString:name])
			return u;
	}
	XUniform *u = [XUniform new];
	u->name = name;
	u->voff = u->foff = -1;
	u->vtex = u->vsmp = u->ftex = u->fsmp = -1;
	[p->uniforms addObject:u];
	return u;
}

void glLinkProgram(GLuint program)
{
	XProgram *p = object(program, [XProgram class]);
	if (!p)
		return;
	p->linked = false;
	if (!p->vs || !p->fs || !p->vs->fn || !p->fs->fn) {
		p->log = @"missing or uncompiled shader";
		return;
	}
	for (XDecl *in in p->fs->ins) {
		XDecl *match = nil;
		for (XDecl *out in p->vs->outs) {
			if ([out->name isEqualToString:in->name])
				match = out;
		}
		if (!match || match->index != in->index) {
			p->log = [NSString stringWithFormat:@"varying %@: no vertex output at location %d",
				  in->name, in->index];
			return;
		}
	}
	p->uniforms = [NSMutableArray array];
	XShader *stages[2] = { p->vs, p->fs };
	for (int i = 0; i < 2; i++) {
		bool vert = i == 0;
		for (XDecl *d in stages[i]->uniforms) {
			XUniform *u = program_uniform(p, d->name);
			u->count = d->count;
			u->comps = d->comps;
			u->cols = d->cols;
			if (vert) {
				u->voff = d->offset;
				u->vstride = d->stride;
			} else {
				u->foff = d->offset;
				u->fstride = d->stride;
			}
		}
		for (XDecl *d in stages[i]->textures) {
			XUniform *u = program_uniform(p, d->name);
			u->sampler = true;
			u->count = 1;
			if (vert) {
				u->vtex = d->index;
				u->vsmp = d->sampler;
			} else {
				u->ftex = d->index;
				u->fsmp = d->sampler;
			}
		}
	}
	p->vbuf = [NSMutableData dataWithLength:p->vs->ubo >= 0 ? p->vs->ubo_size : 0];
	p->fbuf = [NSMutableData dataWithLength:p->fs->ubo >= 0 ? p->fs->ubo_size : 0];
	[p->pipelines removeAllObjects];
	p->linked = true;
}

void glGetProgramiv(GLuint program, GLenum pname, GLint *params)
{
	XProgram *p = object(program, [XProgram class]);
	if (pname == GL_LINK_STATUS)
		*params = p && p->linked ? GL_TRUE : GL_FALSE;
	else if (pname == GL_INFO_LOG_LENGTH)
		*params = p && p->log ? (GLint)strlen(p->log.UTF8String) + 1 : 0;
}

void glGetProgramInfoLog(GLuint program, GLsizei bufsize, GLsizei *length, GLchar *infolog)
{
	XProgram *p = object(program, [XProgram class]);
	copy_log(p ? p->log : nil, bufsize, length, infolog);
}

void glDeleteProgram(GLuint program)
{
	if (object(program, [XProgram class]))
		[objects removeObjectForKey:@(program)];
	if (current_program == program)
		current_program = 0;
}

void glUseProgram(GLuint program)
{
	current_program = program;
}

// Locations: uniform index | array element << 16.
GLint glGetUniformLocation(GLuint program, const GLchar *name)
{
	XProgram *p = object(program, [XProgram class]);
	if (!p || !p->linked)
		return -1;
	NSString *n = [NSString stringWithUTF8String:name];
	int element = 0;
	NSRange br = [n rangeOfString:@"["];
	if (br.location != NSNotFound) {
		element = [n substringFromIndex:br.location + 1].intValue;
		n = [n substringToIndex:br.location];
	}
	for (NSUInteger i = 0; i < p->uniforms.count; i++) {
		XUniform *u = p->uniforms[i];
		if ([u->name isEqualToString:n])
			return element < u->count ? (GLint)(i | (element << 16)) : -1;
	}
	return -1;
}

GLint glGetAttribLocation(GLuint program, const GLchar *name)
{
	XProgram *p = object(program, [XProgram class]);
	if (!p || !p->linked)
		return -1;
	NSString *n = [NSString stringWithUTF8String:name];
	for (XDecl *d in p->vs->attrs) {
		if ([d->name isEqualToString:n])
			return d->index;
	}
	return -1;
}

GLuint glGetUniformBlockIndex(GLuint program, const GLchar *name) { UNSUPPORTED(); return 0; }
void glUniformBlockBinding(GLuint program, GLuint index, GLuint binding) { UNSUPPORTED(); }

static XUniform *uniform_at(GLint location, int *element)
{
	XProgram *p = object(current_program, [XProgram class]);
	if (location < 0 || !p || !p->linked)
		return nil;
	int i = location & 0xffff;
	if (i >= (int)p->uniforms.count)
		return nil;
	*element = location >> 16;
	return p->uniforms[i];
}

// Stores count values of n columns of comps 4-byte components each.
static void set_uniform(GLint location, const void *data, int comps, int cols, int count)
{
	int element;
	XUniform *u = uniform_at(location, &element);
	if (!u)
		return;
	if (u->sampler) {
		u->unit = *(const GLint *)data;
		return;
	}
	if (comps != u->comps || cols != u->cols)
		return;  // a GL error
	XProgram *p = object(current_program, [XProgram class]);
	struct { NSMutableData *buf; int off, stride; } stage[2] = {
		{ p->vbuf, u->voff, u->vstride },
		{ p->fbuf, u->foff, u->fstride },
	};
	int col_bytes = comps * 4;
	// std140: matrix columns and array elements start on 16 bytes
	int col_stride = cols > 1 ? 16 : col_bytes;
	for (int s = 0; s < 2; s++) {
		if (stage[s].off < 0)
			continue;
		uint8_t *base = (uint8_t *)stage[s].buf.mutableBytes;
		const uint8_t *src = data;
		for (int e = element; e < u->count && e < element + count; e++) {
			uint8_t *dst = base + stage[s].off + e * stage[s].stride;
			for (int c = 0; c < cols; c++)
				memcpy(dst + c * col_stride, src + ((e - element) * cols + c) * col_bytes, col_bytes);
		}
	}
}

void glUniform1i(GLint location, GLint v0)
{
	set_uniform(location, &v0, 1, 1, 1);
}

void glUniform1f(GLint location, GLfloat v0)
{
	set_uniform(location, &v0, 1, 1, 1);
}

void glUniform2f(GLint location, GLfloat v0, GLfloat v1)
{
	GLfloat v[2] = { v0, v1 };
	set_uniform(location, v, 2, 1, 1);
}

void glUniform3f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
	GLfloat v[3] = { v0, v1, v2 };
	set_uniform(location, v, 3, 1, 1);
}

void glUniform4f(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)
{
	GLfloat v[4] = { v0, v1, v2, v3 };
	set_uniform(location, v, 4, 1, 1);
}

void glUniform2fv(GLint location, GLsizei count, const GLfloat *value)
{
	set_uniform(location, value, 2, 1, count);
}

void glUniform3fv(GLint location, GLsizei count, const GLfloat *value)
{
	set_uniform(location, value, 3, 1, count);
}

void glUniform4fv(GLint location, GLsizei count, const GLfloat *value)
{
	set_uniform(location, value, 4, 1, count);
}

void glUniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
	if (transpose)
		UNSUPPORTED();
	set_uniform(location, value, 3, 3, count);
}

void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)
{
	if (transpose)
		UNSUPPORTED();
	set_uniform(location, value, 4, 4, count);
}

void glDepthMask(GLboolean flag) { UNSUPPORTED(); }
void glDepthFunc(GLenum func) { UNSUPPORTED(); }
void glCullFace(GLenum mode) { UNSUPPORTED(); }
void glFrontFace(GLenum mode) { UNSUPPORTED(); }
void glPolygonOffset(GLfloat factor, GLfloat units) { UNSUPPORTED(); }

/* --- drawing --- */

static MTLBlendFactor blend_factor(GLenum f)
{
	switch (f) {
	case GL_ZERO: return MTLBlendFactorZero;
	case GL_ONE: return MTLBlendFactorOne;
	case GL_SRC_COLOR: return MTLBlendFactorSourceColor;
	case GL_ONE_MINUS_SRC_COLOR: return MTLBlendFactorOneMinusSourceColor;
	case GL_SRC_ALPHA: return MTLBlendFactorSourceAlpha;
	case GL_ONE_MINUS_SRC_ALPHA: return MTLBlendFactorOneMinusSourceAlpha;
	case GL_DST_ALPHA: return MTLBlendFactorDestinationAlpha;
	case GL_ONE_MINUS_DST_ALPHA: return MTLBlendFactorOneMinusDestinationAlpha;
	case GL_DST_COLOR: return MTLBlendFactorDestinationColor;
	case GL_ONE_MINUS_DST_COLOR: return MTLBlendFactorOneMinusDestinationColor;
	case GL_SRC_ALPHA_SATURATE: return MTLBlendFactorSourceAlphaSaturated;
	case GL_CONSTANT_COLOR: return MTLBlendFactorBlendColor;
	case GL_ONE_MINUS_CONSTANT_COLOR: return MTLBlendFactorOneMinusBlendColor;
	case GL_CONSTANT_ALPHA: return MTLBlendFactorBlendAlpha;
	case GL_ONE_MINUS_CONSTANT_ALPHA: return MTLBlendFactorOneMinusBlendAlpha;
	}
	ERROR("blend factor 0x%x", f);
}

static MTLBlendOperation blend_op(GLenum e)
{
	switch (e) {
	case GL_FUNC_ADD: return MTLBlendOperationAdd;
	case GL_FUNC_SUBTRACT: return MTLBlendOperationSubtract;
	case GL_FUNC_REVERSE_SUBTRACT: return MTLBlendOperationReverseSubtract;
	case GL_MIN: return MTLBlendOperationMin;
	case GL_MAX: return MTLBlendOperationMax;
	}
	ERROR("blend equation 0x%x", e);
}

static id<MTLRenderPipelineState> pipeline(XProgram *p, id<MTLTexture> target, bool rgb, int stride)
{
	NSString *key = [NSString stringWithFormat:@"%d %d %d %x %x %x %x %x %x %d",
			 (int)target.pixelFormat, rgb, blend.enabled, blend.src_rgb, blend.dst_rgb,
			 blend.src_alpha, blend.dst_alpha, blend.eq_rgb, blend.eq_alpha, stride];
	for (XDecl *a in p->vs->attrs)
		key = [key stringByAppendingFormat:@" %d:%d:%lu", a->index, attribs[a->index].size,
		       (unsigned long)attribs[a->index].offset];
	id<MTLRenderPipelineState> state = p->pipelines[key];
	if (state)
		return state;

	MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
	d.vertexFunction = p->vs->fn;
	d.fragmentFunction = p->fs->fn;
	MTLVertexDescriptor *vd = [MTLVertexDescriptor vertexDescriptor];
	for (XDecl *a in p->vs->attrs) {
		static const MTLVertexFormat formats[] = {
			MTLVertexFormatInvalid, MTLVertexFormatFloat, MTLVertexFormatFloat2,
			MTLVertexFormatFloat3, MTLVertexFormatFloat4
		};
		vd.attributes[a->index].format = formats[attribs[a->index].size];
		vd.attributes[a->index].offset = attribs[a->index].offset;
		vd.attributes[a->index].bufferIndex = VERTEX_BUFFER_INDEX;
	}
	vd.layouts[VERTEX_BUFFER_INDEX].stride = stride;
	d.vertexDescriptor = vd;
	MTLRenderPipelineColorAttachmentDescriptor *c = d.colorAttachments[0];
	c.pixelFormat = target.pixelFormat;
	c.writeMask = rgb ? (MTLColorWriteMaskRed | MTLColorWriteMaskGreen | MTLColorWriteMaskBlue)
		: MTLColorWriteMaskAll;
	c.blendingEnabled = blend.enabled;
	c.sourceRGBBlendFactor = blend_factor(blend.src_rgb);
	c.destinationRGBBlendFactor = blend_factor(blend.dst_rgb);
	c.sourceAlphaBlendFactor = blend_factor(blend.src_alpha);
	c.destinationAlphaBlendFactor = blend_factor(blend.dst_alpha);
	c.rgbBlendOperation = blend_op(blend.eq_rgb);
	c.alphaBlendOperation = blend_op(blend.eq_alpha);
	NSError *err = nil;
	state = [device newRenderPipelineStateWithDescriptor:d error:&err];
	if (!state)
		ERROR("Metal pipeline: %s", err.localizedDescription.UTF8String);
	p->pipelines[key] = state;
	return state;
}

static id<MTLSamplerState> sampler_for(XTex *t)
{
	GLenum minf = t->min_filter, magf = t->mag_filter, ws = t->wrap_s, wt = t->wrap_t;
	bool min_linear = minf == GL_LINEAR || minf == GL_LINEAR_MIPMAP_NEAREST || minf == GL_LINEAR_MIPMAP_LINEAR;
	bool mag_linear = magf == GL_LINEAR;
	NSNumber *key = @((min_linear ? 1 : 0) | (mag_linear ? 2 : 0) | ((ws & 0xffff) << 4) | ((uint64_t)(wt & 0xffff) << 20));
	id<MTLSamplerState> s = samplers[key];
	if (s)
		return s;
	MTLSamplerDescriptor *d = [MTLSamplerDescriptor new];
	d.minFilter = min_linear ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
	d.magFilter = mag_linear ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
	MTLSamplerAddressMode modes[2];
	GLenum wraps[2] = { ws, wt };
	for (int i = 0; i < 2; i++) {
		modes[i] = wraps[i] == GL_CLAMP_TO_EDGE ? MTLSamplerAddressModeClampToEdge
			: wraps[i] == GL_MIRRORED_REPEAT ? MTLSamplerAddressModeMirrorRepeat
			: MTLSamplerAddressModeRepeat;
	}
	d.sAddressMode = modes[0];
	d.tAddressMode = modes[1];
	s = [device newSamplerStateWithDescriptor:d];
	samplers[key] = s;
	return s;
}

static void bind_textures(XProgram *p)
{
	for (XUniform *u in p->uniforms) {
		if (!u->sampler)
			continue;
		XTex *t = u->unit >= 0 && u->unit < MAX_UNITS ? object(units[u->unit], [XTex class]) : nil;
		id<MTLTexture> tex = t && t->tex ? t->tex : dummy_texture;
		id<MTLSamplerState> smp = sampler_for(t ? t : (XTex *)[XTex new]);
		if (u->vtex >= 0) {
			[encoder setVertexTexture:tex atIndex:u->vtex];
			[encoder setVertexSamplerState:smp atIndex:u->vsmp];
		}
		if (u->ftex >= 0) {
			[encoder setFragmentTexture:tex atIndex:u->ftex];
			[encoder setFragmentSamplerState:smp atIndex:u->fsmp];
		}
	}
}

// Draws count vertices: indices[i], or first + i without indices.
static void draw(GLenum mode, GLsizei count, const uint32_t *indices, GLint first)
{
	XProgram *p = object(current_program, [XProgram class]);
	if (!p || !p->linked || count <= 0)
		return;
	XTex *t;
	id<MTLTexture> target = fb_texture(draw_fb, &t);
	if (!target)
		return;
	bool rgb = t && t->format == GL_RGB;

	// the vertices, in the order Metal draws them
	XBuf *vb = nil;
	int stride = 0;
	for (XDecl *a in p->vs->attrs) {
		if (!attribs[a->index].enabled)
			ERROR("vertex attribute %s not enabled", a->name.UTF8String);
		XBuf *b = object(attribs[a->index].buffer, [XBuf class]);
		if ((vb && b != vb) || (stride && attribs[a->index].stride != stride))
			UNSUPPORTED();
		vb = b;
		stride = attribs[a->index].stride;
	}
	if (!vb || !stride)
		return;
	MTLPrimitiveType prim;
	int n;
	switch (mode) {
	case GL_TRIANGLE_FAN:
		prim = MTLPrimitiveTypeTriangle;
		n = count >= 3 ? (count - 2) * 3 : 0;
		break;
	case GL_TRIANGLES: prim = MTLPrimitiveTypeTriangle; n = count; break;
	case GL_TRIANGLE_STRIP: prim = MTLPrimitiveTypeTriangleStrip; n = count; break;
	case GL_LINES: prim = MTLPrimitiveTypeLine; n = count; break;
	default: UNSUPPORTED(); return;
	}
	if (!n)
		return;
	NSMutableData *verts = [NSMutableData dataWithLength:(size_t)n * stride];
	uint8_t *dst = verts.mutableBytes;
	const uint8_t *src = vb->data.bytes;
	size_t src_len = vb->data.length;
	for (int i = 0; i < n; i++) {
		int k = i;
		if (mode == GL_TRIANGLE_FAN)
			k = i % 3 == 0 ? 0 : i / 3 + i % 3;
		uint32_t v = indices ? indices[k] : (uint32_t)(first + k);
		if ((size_t)(v + 1) * stride > src_len)
			ERROR("vertex %u outside the buffer", v);
		memcpy(dst + (size_t)i * stride, src + (size_t)v * stride, stride);
	}

	id<MTLRenderPipelineState> state = pipeline(p, target, rgb, stride);
	if (encoder_target != target)
		begin_pass(target, false, MTLClearColorMake(0, 0, 0, 0));
	[encoder setRenderPipelineState:state];
	bool window_fb = !draw_fb;
	double oy = window_fb ? (double)target.height - viewport.y - viewport.h : viewport.y;
	[encoder setViewport:(MTLViewport){ viewport.x, oy, viewport.w, viewport.h, 0, 1 }];
	[encoder setBlendColorRed:blend.color[0] green:blend.color[1] blue:blend.color[2] alpha:blend.color[3]];
	float flip[4] = { window_fb ? 1 : -1, 0, 0, 0 };
	[encoder setVertexBytes:flip length:sizeof(flip) atIndex:FLIP_BUFFER_INDEX];
	if (verts.length <= 4096)
		[encoder setVertexBytes:verts.bytes length:verts.length atIndex:VERTEX_BUFFER_INDEX];
	else
		[encoder setVertexBuffer:[device newBufferWithBytes:verts.bytes length:verts.length options:0]
				  offset:0 atIndex:VERTEX_BUFFER_INDEX];
	if (p->vs->ubo >= 0)
		[encoder setVertexBytes:p->vbuf.bytes length:p->vbuf.length atIndex:p->vs->ubo];
	if (p->fs->ubo >= 0)
		[encoder setFragmentBytes:p->fbuf.bytes length:p->fbuf.length atIndex:p->fs->ubo];
	bind_textures(p);
	[encoder drawPrimitives:prim vertexStart:0 vertexCount:n];
}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices)
{
	XBuf *ib = object(element_buffer, [XBuf class]);
	if (!ib || type != GL_UNSIGNED_INT)
		UNSUPPORTED();
	uintptr_t off = (uintptr_t)indices;
	if (off + (size_t)count * 4 > ib->data.length)
		ERROR("glDrawElements: indices outside the buffer");
	@autoreleasepool {
		draw(mode, count, (const uint32_t *)((const uint8_t *)ib->data.bytes + off), 0);
	}
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
	@autoreleasepool {
		draw(mode, count, NULL, first);
	}
}
