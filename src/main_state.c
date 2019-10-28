#include <main_state.h>
#include <glad/glad.h>
#include <math.h>

#include <rafgl.h>
#include <game_constants.h>

static rafgl_raster_t doge, upscaled_doge, raster, raster2;
static rafgl_texture_t texture;
static int raster_width = RASTER_WIDTH, raster_height = RASTER_HEIGHT;
static char save_file[256];
static int save_file_no = 0;

void main_state_init(GLFWwindow *window, void *args) {
    rafgl_raster_load_from_image(&doge, "res/images/doge.png");

    rafgl_raster_init(&upscaled_doge, raster_width, raster_height);
    rafgl_raster_bilinear_upsample(&upscaled_doge, &doge);

    rafgl_raster_init(&raster, raster_width, raster_height);
    rafgl_raster_init(&raster2, raster_width, raster_height);

    rafgl_texture_init(&texture);
}

int pressed;
float location = 0;
float selector = 0;

void main_state_update(GLFWwindow *window, float delta_time, rafgl_game_data_t *game_data, void *args) {
    if(game_data->is_lmb_down && game_data->is_rmb_down) {
        pressed = 1;
        location = rafgl_clampf(game_data->mouse_pos_y, 0, raster_height - 1);
        selector = 1.0f * location / raster_height;
    }else {
        pressed = 0;
    }

	int x, y;

    float xn, yn;

    rafgl_pixel_rgb_t sampled, sampled2, resulting, resulting2;

    for(y = 0; y < raster_height; y++) {
        yn = 1.0f * y / raster_height;

        for(x = 0; x < raster_width; x++) {
            xn = 1.0f * x / raster_width;

            sampled = pixel_at_m(upscaled_doge, x, y);
            sampled2 = rafgl_point_sample(&doge, xn, yn);

            resulting = sampled;
            resulting2 = sampled2;

            resulting.rgba = rafgl_RGB(0, 0, 0);

            pixel_at_m(raster, x, y) = resulting;
            pixel_at_m(raster2, x, y) = resulting2;

            if(pressed && rafgl_distance1D(location, y) < 3 && x > raster_width - 15) {
                pixel_at_m(raster, x, y).rgba = rafgl_RGB(255, 0, 0);
            }
        }
    }
    
	if(game_data->keys_pressed[RAFGL_KEY_S] && game_data->keys_down[RAFGL_KEY_LEFT_SHIFT]) {
        sprintf(save_file, "save%d.png", save_file_no++);
        rafgl_raster_save_to_png(&raster, save_file);
    }

    if(!game_data->keys_down[RAFGL_KEY_SPACE]) {
        rafgl_texture_load_from_raster(&texture, &raster);
	}else {
        rafgl_texture_load_from_raster(&texture, &raster2);
	}
}

void main_state_render(GLFWwindow *window, void *args) {
    rafgl_texture_show(&texture);
}

void main_state_cleanup(GLFWwindow *window, void *args) {
    rafgl_raster_cleanup(&raster);
    rafgl_raster_cleanup(&raster2);
    rafgl_texture_cleanup(&texture);
}
