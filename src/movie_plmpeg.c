/* Copyright (C) 2023 kichikuou <KichikuouChrome@gmail.com>
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

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>
#include <sndfile.h>

#include "system4.h"
#include "system4/file.h"

#include "apeg.h"
#include "gfx/gfx.h"
#include "movie.h"
#include "mixer.h"
#include "sprite.h"
#include "sts_mixer.h"
#include "xsystem4.h"

#define PL_MPEG_IMPLEMENTATION
#include "pl_mpeg.h"

static Shader movie_shader;

struct movie_context {
	plm_t *plm;
	SDL_Mutex *decoder_mutex;

	plm_frame_t *pending_video_frame;
	GLuint textures[3];  // Y, Cb, Cr

	sts_mixer_stream_t sts_stream;
	int voice;  // the mixer's stream handle
	int volume;
	int sound_group;  // movie_set_sound_group; -1: the master mixer

	// Time keeping data. Written by audio handler and referenced by video
	// handler (i.e. we sync the video to the audio).
	double stream_time;  // in seconds
	uint32_t wall_time_ms;
	SDL_Mutex *timer_mutex;

	// APEG (AliceSoft's movie format; plm is NULL): src/apeg.c decodes the
	// pictures on the drawing thread, libsndfile the Ogg Vorbis track (the
	// first SOND chunk), read in place from the file, on the audio thread.
	struct apeg *apeg;
	FILE *snd_fp;
	uint64_t snd_offset;
	sf_count_t snd_size, snd_pos;  // the chunk and the read position in it
	SNDFILE *snd;
	SF_INFO snd_info;
	sf_count_t snd_frames;  // frames handed to the mixer so far
	float *snd_in, *snd_out;
	bool playing;
	bool audio_finished;
	int start_ms;          // movie_seek
	uint8_t *apeg_rgba;
	int apeg_shown;        // picture in the last texture drawn, -1 for none
	unsigned apeg_shown_handle;  // handle of that texture
	int apeg_time;         // the latest time a picture was picked for
	unsigned apeg_errors;  // pictures that could not be shown
	bool apeg_size_warned;
	bool black_frame_drawn;
	struct texture temp_surface;  // movie_draw without a sprite
};

#define APEG_AUDIO_FRAMES 4096

static sf_count_t snd_vio_get_filelen(void *data)
{
	return ((struct movie_context *)data)->snd_size;
}

static sf_count_t snd_vio_seek(sf_count_t offset, int whence, void *data)
{
	struct movie_context *mc = data;
	switch (whence) {
	case SEEK_SET: break;
	case SEEK_CUR: offset += mc->snd_pos; break;
	case SEEK_END: offset += mc->snd_size; break;
	default: return -1;
	}
	if (offset < 0 || offset > mc->snd_size)
		return -1;
	mc->snd_pos = offset;
	return offset;
}

static sf_count_t snd_vio_read(void *ptr, sf_count_t count, void *data)
{
	struct movie_context *mc = data;
	if (count > mc->snd_size - mc->snd_pos)
		count = mc->snd_size - mc->snd_pos;
	if (count <= 0 || fseeko(mc->snd_fp, (off_t)(mc->snd_offset + mc->snd_pos), SEEK_SET))
		return 0;
	size_t got = fread(ptr, 1, (size_t)count, mc->snd_fp);
	mc->snd_pos += got;
	return got;
}

static sf_count_t snd_vio_write(possibly_unused const void *ptr,
		possibly_unused sf_count_t count, possibly_unused void *data)
{
	return 0;
}

static sf_count_t snd_vio_tell(void *data)
{
	return ((struct movie_context *)data)->snd_pos;
}

static SF_VIRTUAL_IO snd_vio = {
	.get_filelen = snd_vio_get_filelen,
	.seek = snd_vio_seek,
	.read = snd_vio_read,
	.write = snd_vio_write,
	.tell = snd_vio_tell
};

// The APEG sound, as stereo float (a mono track goes to both sides).
static int apeg_audio_callback(sts_mixer_sample_t *sample, void *data)
{
	struct movie_context *mc = data;
	int ch = mc->snd_info.channels;
	sf_count_t n = sf_readf_float(mc->snd, mc->snd_in, APEG_AUDIO_FRAMES);
	if (n <= 0) {
		sample->length = 0;
		sample->data = NULL;
		mc->voice = -1;
		mc->audio_finished = true;
		return STS_STREAM_COMPLETE;
	}
	for (sf_count_t i = 0; i < n; i++) {
		const float *in = &mc->snd_in[i * ch];
		mc->snd_out[i*2] = in[0];
		mc->snd_out[i*2+1] = ch > 1 ? in[1] : in[0];
	}
	sample->length = n * 2;
	sample->data = mc->snd_out;

	SDL_LockMutex(mc->timer_mutex);
	mc->stream_time = (double)mc->snd_frames / mc->snd_info.samplerate;
	mc->wall_time_ms = SDL_GetTicks();
	SDL_UnlockMutex(mc->timer_mutex);
	mc->snd_frames += n;
	return STS_STREAM_CONTINUE;
}

// Opens an APEG's pictures and its sound. A file without a SOND chunk plays
// on the wall clock for the header's duration.
static bool apeg_load(struct movie_context *mc, const char *path)
{
	const char *err = NULL;
	mc->apeg = apeg_open(path, &err);
	if (!mc->apeg) {
		WARNING("%s: %s", path, err);
		return false;
	}
	if (apeg_warning(mc->apeg))
		WARNING("%s: %s", path, apeg_warning(mc->apeg));
	mc->apeg_shown = -1;

	uint64_t offset;
	uint32_t size;
	if (!apeg_sound(mc->apeg, &offset, &size)) {
		WARNING("%s: no sound track", path);
		return true;
	}
	mc->snd_fp = file_open_utf8(path, "rb");
	if (!mc->snd_fp) {
		WARNING("%s: %s", path, strerror(errno));
		return false;
	}
	mc->snd_offset = offset;
	mc->snd_size = size;
	mc->snd = sf_open_virtual(&snd_vio, SFM_READ, &mc->snd_info, mc);
	if (!mc->snd || mc->snd_info.channels < 1 || mc->snd_info.samplerate <= 0) {
		WARNING("%s: sound track: %s", path, sf_strerror(mc->snd));
		return false;
	}
	mc->snd_in = xmalloc(sizeof(float) * APEG_AUDIO_FRAMES * mc->snd_info.channels);
	mc->snd_out = xmalloc(sizeof(float) * APEG_AUDIO_FRAMES * 2);
	return true;
}

static int audio_callback(sts_mixer_sample_t *sample, void *data)
{
	struct movie_context *mc = data;
	assert(sample == &mc->sts_stream.sample);

	SDL_LockMutex(mc->decoder_mutex);
	plm_samples_t *frame = plm_decode_audio(mc->plm);
	SDL_UnlockMutex(mc->decoder_mutex);
	if (!frame) {
		sample->length = 0;
		sample->data = NULL;
		mc->voice = -1;
		return STS_STREAM_COMPLETE;
	}
	sample->length = frame->count * 2;
	sample->data = frame->interleaved;

	// Update the timestamp.
	SDL_LockMutex(mc->timer_mutex);
	mc->stream_time = frame->time;
	mc->wall_time_ms = SDL_GetTicks();
	SDL_UnlockMutex(mc->timer_mutex);
	return STS_STREAM_CONTINUE;
}

static void prepare_movie_shader(struct gfx_render_job *job, void *data)
{
	struct movie_context *mc = data;
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mc->textures[0]);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, mc->textures[1]);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, mc->textures[2]);
}

static void load_movie_shader()
{
	gfx_load_shader(&movie_shader, "shaders/render.v.glsl", "shaders/movie.f.glsl");
	glUseProgram(movie_shader.program);
	glUniform1i(glGetUniformLocation(movie_shader.program, "texture_y"), 0);
	glUniform1i(glGetUniformLocation(movie_shader.program, "texture_cb"), 1);
	glUniform1i(glGetUniformLocation(movie_shader.program, "texture_cr"), 2);
	movie_shader.prepare = prepare_movie_shader;
}

static void update_texture(GLuint unit, GLuint texture, plm_plane_t *plane)
{
	glActiveTexture(unit);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, plane->width, plane->height, 0,
	             GL_RED, GL_UNSIGNED_BYTE, plane->data);
}

struct movie_context *movie_load(const char *filename)
{
	struct movie_context *mc = xcalloc(1, sizeof(struct movie_context));
	mc->voice = -1;
	mc->sound_group = -1;
	char *path = gamedir_path_icase(filename);
	if (!path) {
		WARNING("%s: file does not exist", filename);
		movie_free(mc);
		return NULL;
	}
	FILE *fp = file_open_utf8(path, "rb");
	if (!fp) {
		WARNING("%s: %s", path, strerror(errno));
		free(path);
		movie_free(mc);
		return NULL;
	}
	char magic[4];
	if (fread(magic, 1, 4, fp) == 4 && !memcmp(magic, "APEG", 4)) {
		fclose(fp);
		mc->timer_mutex = SDL_CreateMutex();
		mc->volume = 100;
		bool ok = apeg_load(mc, path);
		free(path);
		if (!ok) {
			movie_free(mc);
			return NULL;
		}
		return mc;
	}
	rewind(fp);
	mc->plm = plm_create_with_file(fp, TRUE);
	if (!plm_has_headers(mc->plm)) {
		WARNING("%s: not a MPEG-PS file", path);
		free(path);
		movie_free(mc);
		return NULL;
	}
	free(path);

	if (!movie_shader.program)
		load_movie_shader();

	glGenTextures(3, mc->textures);
	for (int i = 0; i < 3; i++) {
		glBindTexture(GL_TEXTURE_2D, mc->textures[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	mc->decoder_mutex = SDL_CreateMutex();
	mc->timer_mutex = SDL_CreateMutex();
	mc->volume = 100;
	return mc;
}

void movie_free(struct movie_context *mc)
{
	if (mc->voice >= 0)
		mixer_stream_stop(mc->voice);
	mixer_movie_sound_release(mc);

	if (mc->plm)
		plm_destroy(mc->plm);
	if (mc->snd)
		sf_close(mc->snd);
	if (mc->snd_fp)
		fclose(mc->snd_fp);
	free(mc->snd_in);
	free(mc->snd_out);
	if (mc->apeg)
		apeg_close(mc->apeg);
	free(mc->apeg_rgba);
	if (mc->temp_surface.handle)
		gfx_delete_texture(&mc->temp_surface);
	if (mc->textures[0])
		glDeleteTextures(3, mc->textures);
	if (mc->decoder_mutex)
		SDL_DestroyMutex(mc->decoder_mutex);
	if (mc->timer_mutex)
		SDL_DestroyMutex(mc->timer_mutex);
	free(mc);
}

static bool apeg_play(struct movie_context *mc)
{
	mc->stream_time = mc->start_ms / 1000.0;
	mc->wall_time_ms = SDL_GetTicks();
	mc->apeg_time = INT_MIN;
	mc->playing = true;
	if (!mc->snd)
		return true;
	mc->sts_stream.userdata = mc;
	mc->sts_stream.callback = apeg_audio_callback;
	mc->sts_stream.sample.frequency = mc->snd_info.samplerate;
	mc->sts_stream.sample.audio_format = STS_MIXER_SAMPLE_FORMAT_FLOAT;
	mc->voice = mixer_stream_play_group(&mc->sts_stream, mc->volume, mc->sound_group);
	return true;
}

bool movie_play(struct movie_context *mc)
{
	if (mc->apeg)
		return apeg_play(mc);
	// Start the audio stream.
	mc->stream_time = 0.0;
	mc->wall_time_ms = SDL_GetTicks();
	mc->sts_stream.userdata = mc;
	mc->sts_stream.callback = audio_callback;
	mc->sts_stream.sample.frequency = plm_get_samplerate(mc->plm);
	mc->sts_stream.sample.audio_format = STS_MIXER_SAMPLE_FORMAT_FLOAT;
	mc->voice = mixer_stream_play_group(&mc->sts_stream, mc->volume, mc->sound_group);
	return true;
}

void movie_set_sound_group(struct movie_context *mc, int sound_id, int sound_group)
{
	mc->sound_group = mixer_movie_sound_set(sound_id, sound_group, mc);
}

static bool apeg_is_end(struct movie_context *mc)
{
	if (mc->snd)
		return mc->audio_finished;
	return mc->playing && movie_get_position(mc) >= apeg_duration(mc->apeg);
}

// The APEG picture due at the audio clock: the latest one whose timestamp is
// not later than it (the original's 0x44a1b0), converted and uploaded when it
// is not the one tex already shows. The clock is made monotonic here: it can
// read a few ms less than before between two audio callbacks, and the
// original never shows an older picture again.
static bool apeg_update_texture(struct movie_context *mc, struct texture *tex)
{
	int w = apeg_width(mc->apeg), h = apeg_height(mc->apeg);
	if (tex->w != w || tex->h != h) {
		if (!mc->apeg_size_warned)
			WARNING("APEG %dx%d cannot be drawn into a %dx%d texture", w, h, tex->w, tex->h);
		mc->apeg_size_warned = true;
		return false;
	}
	int t = movie_get_position(mc);
	if (t < mc->apeg_time)
		t = mc->apeg_time;
	mc->apeg_time = t;
	const struct apeg_picture *p = apeg_picture_at(mc->apeg, t);
	if (!p) {
		// Before the first picture, or a picture that cannot be decoded:
		// the texture keeps what it shows.
		const char *err = apeg_error(mc->apeg);
		if (err) {
			mc->apeg_errors++;
			if (mc->apeg_errors <= 8 || !(mc->apeg_errors & 255))
				WARNING("APEG picture at %d ms: %s (%u failures so far)", t, err, mc->apeg_errors);
		}
		return false;
	}
	if (p->index == mc->apeg_shown && tex->handle == mc->apeg_shown_handle)
		return false;
	if (!mc->apeg_rgba)
		mc->apeg_rgba = xmalloc((size_t)p->width * p->height * 4);
	apeg_picture_to_rgba(mc->apeg, p, mc->apeg_rgba, (size_t)p->width * 4);
	gfx_update_texture_with_pixels(tex, mc->apeg_rgba);
	mc->apeg_shown = p->index;
	mc->apeg_shown_handle = tex->handle;
	return true;
}

static bool apeg_draw(struct movie_context *mc, struct sact_sprite *sprite)
{
	if (sprite) {
		struct texture *tex = sprite_get_texture(sprite);
		if (movie_update_texture(mc, tex)) {
			sprite_dirty(sprite);
		} else if (mc->apeg_shown < 0 && !mc->black_frame_drawn && tex->w > 0 && tex->h > 0) {
			// Black until the first picture.
			mc->black_frame_drawn = true;
			uint32_t *black = xcalloc((size_t)tex->w * tex->h, sizeof(uint32_t));
			gfx_update_texture_with_pixels(tex, black);
			free(black);
			sprite_dirty(sprite);
		}
	} else {
		// Directly to the main framebuffer, at the movie's size.
		if (!mc->temp_surface.handle)
			gfx_init_texture_blank(&mc->temp_surface, apeg_width(mc->apeg), apeg_height(mc->apeg));
		if (movie_update_texture(mc, &mc->temp_surface)) {
			gfx_clear();
			gfx_render_texture(&mc->temp_surface, NULL);
			gfx_swap();
		}
	}
	return !apeg_is_end(mc);
}

bool movie_draw(struct movie_context *mc, struct sact_sprite *sprite)
{
	if (mc->apeg)
		return apeg_draw(mc, sprite);
	// Decode a frame, unless we already have one.
	plm_frame_t *frame = mc->pending_video_frame;
	mc->pending_video_frame = NULL;
	if (!frame) {
		SDL_LockMutex(mc->decoder_mutex);
		frame = plm_decode_video(mc->plm);
		SDL_UnlockMutex(mc->decoder_mutex);
		if (!frame)
			return plm_video_has_ended(mc->plm->video_decoder);
	}

	// If the frame's timestamp is in the future, save the frame and return.
	SDL_LockMutex(mc->timer_mutex);
	double now = mc->stream_time + (SDL_GetTicks() - mc->wall_time_ms) / 1000.0;
	SDL_UnlockMutex(mc->timer_mutex);
	if (frame->time > now) {
		mc->pending_video_frame = frame;
		return true;
	}

	// Render the frame.
	update_texture(GL_TEXTURE0, mc->textures[0], &frame->y);
	update_texture(GL_TEXTURE1, mc->textures[1], &frame->cb);
	update_texture(GL_TEXTURE2, mc->textures[2], &frame->cr);

	float w, h;
	GLuint fbo;
	if (sprite) {
		struct texture *texture = sprite_get_texture(sprite);
		w = texture->w;
		h = texture->h;
		fbo = gfx_set_framebuffer(GL_DRAW_FRAMEBUFFER, texture, 0, 0, w, h);
	} else {
		// Draw directly to the main framebuffer.
		w = config.view_width;
		h = config.view_height;
		gfx_clear();
	}

	mat4 world_transform = WORLD_TRANSFORM(w, h, 0, 0);
	mat4 wv_transform = WV_TRANSFORM(w, h);

	struct gfx_render_job job = {
		.shader = &movie_shader,
		.shape = GFX_RECTANGLE,
		.texture = 0,
		.world_transform = world_transform[0],
		.view_transform = wv_transform[0],
		.data = mc
	};
	gfx_render(&job);

	if (sprite) {
		gfx_reset_framebuffer(GL_DRAW_FRAMEBUFFER, fbo);
		sprite_dirty(sprite);
	} else {
		gfx_swap();
	}

	return true;
}

bool movie_is_end(struct movie_context *mc)
{
	if (mc->apeg)
		return apeg_is_end(mc);
	return plm_has_ended(mc->plm);
}

int movie_get_position(struct movie_context *mc)
{
	SDL_LockMutex(mc->timer_mutex);
	int ms = mc->wall_time_ms ? mc->stream_time * 1000 + SDL_GetTicks() - mc->wall_time_ms : 0;
	SDL_UnlockMutex(mc->timer_mutex);
	return ms;
}

bool movie_set_volume(struct movie_context *mc, int volume)
{
	mc->volume = volume;
	if (mc->voice >= 0)
		mixer_stream_set_volume(mc->voice, volume);
	return true;
}

void movie_get_video_size(struct movie_context *mc, int *out_w, int *out_h)
{
	if (mc->apeg) {
		if (out_w) *out_w = apeg_width(mc->apeg);
		if (out_h) *out_h = apeg_height(mc->apeg);
		return;
	}
	if (out_w) *out_w = plm_get_width(mc->plm);
	if (out_h) *out_h = plm_get_height(mc->plm);
}

int movie_get_duration(struct movie_context *mc)
{
	if (mc->apeg)
		return apeg_duration(mc->apeg);
	return (int)(plm_get_duration(mc->plm) * 1000);
}

// Only an APEG can start elsewhere than 0 (its sound track is seeked).
bool movie_seek(struct movie_context *mc, int msec)
{
	if (!mc->apeg || mc->playing)
		return msec == 0;
	if (mc->snd) {
		sf_count_t frame = (sf_count_t)msec * mc->snd_info.samplerate / 1000;
		if (sf_seek(mc->snd, frame, SEEK_SET) < 0)
			return false;
		mc->snd_frames = frame;
	}
	mc->start_ms = msec;
	return true;
}

// MPEG-1 parts movies keep their back colour.
bool movie_update_texture(struct movie_context *mc, struct texture *tex)
{
	if (!mc->apeg || !mc->playing || !tex || !tex->handle)
		return false;
	return apeg_update_texture(mc, tex);
}
