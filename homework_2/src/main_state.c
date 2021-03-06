#include <game_constants.h>
#include <glad/glad.h>
#include <main_state.h>
#include <math.h>
#include <rafgl.h>
#include <stdlib.h>
#include <time.h>

#define TOTAL_STAGES 4
#define TOTAL_UNIFORMS 4
#define TOTAL_MESHES 1
#define LENGTH_NAME 128
#define FIELD_OF_VIEW 75.0f

static struct pipeline_stage {
	GLuint shader;
	GLuint uni[TOTAL_UNIFORMS];
	rafgl_framebuffer_simple_t fbo;
} stages[TOTAL_STAGES];

static mat4_t model, view, projection, view_projection;

static vec3_t camera_dir = vec3m(0.0f, 1.0f, 6.5f);
static vec3_t camera_up = vec3m(0.0f, 1.0f, 0.0f);
static vec3_t aim_dir = vec3m(0.0f, 0.0f, -1.0f);

static rafgl_meshPUN_t meshes[TOTAL_MESHES];
static const char* mesh_names[TOTAL_MESHES] = { "res/models/monkey.obj" };

static const int size_noise = 16;
static const int size_kernel = 64;

static const float factor_scale = 1.0f;

static int current_texture = 0;
static int current_mesh = 0;

static float angle_v = 0.0f;
static float angle_h = -M_PIf * 0.5f;
static float angle_speed = M_PIf * 0.006f;
static float angle_model = -M_PIf;
static float speed_move = 5.0f;

static int flag_rotate = 0;
static float flag_ssao = 1.0f;

static GLuint tex_position, smp_position;
static GLuint tex_normal, smp_normal;
static GLuint tex_color, smp_color;
static GLuint tex_noise, smp_noise;
static GLuint tex_kernel, smp_kernel;
static GLuint tex_ssao, smp_ssao;
static GLuint tex_blur, smp_blur;
static GLuint tex_light;

static GLuint quadVAO, quadVBO;

static float quad[] = {
	// X, Y, Z, U, V
	-1.0f, 1.0f, 0.0f, 0.0f, 1.0f,
	-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
	1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
	1.0f, -1.0f, 0.0f, 1.0f, 0.0f
};

void render_quad() {
	if(!quadVAO) {
		glGenVertexArrays(1, &quadVAO);
		glBindVertexArray(quadVAO);

		glGenBuffers(1, &quadVBO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quad), &quad, GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (const void*) 0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (const void*) (3 * sizeof(float)));
	}

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);

	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void init_geometry(int width, int height) {
	stages[0].shader = rafgl_program_create_from_name("geometry");
	stages[0].uni[0] = glGetUniformLocation(stages[0].shader, "uni_m");
	stages[0].uni[1] = glGetUniformLocation(stages[0].shader, "uni_v");
	stages[0].uni[2] = glGetUniformLocation(stages[0].shader, "uni_p");
	stages[0].fbo = rafgl_framebuffer_simple_create(width, height);

	glBindFramebuffer(GL_FRAMEBUFFER, stages[0].fbo.fbo_id);

	glGenTextures(1, &tex_position);
	glBindTexture(GL_TEXTURE_2D, tex_position);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_position, 0);

	glGenTextures(1, &tex_normal);
	glBindTexture(GL_TEXTURE_2D, tex_normal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, tex_normal, 0);

	glGenTextures(1, &tex_color);
	glBindTexture(GL_TEXTURE_2D, tex_color);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, tex_color, 0);

	GLuint buffers[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };

	glDrawBuffers(3, buffers);
}

void render_geometry() {
	glBindFramebuffer(GL_FRAMEBUFFER, stages[0].fbo.fbo_id);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(stages[0].shader);
	glUniformMatrix4fv(stages[0].uni[0], 1, GL_FALSE, (void*) model.m);
	glUniformMatrix4fv(stages[0].uni[1], 1, GL_FALSE, (void*) view.m);
	glUniformMatrix4fv(stages[0].uni[2], 1, GL_FALSE, (void*) projection.m);

	glBindVertexArray(meshes[current_mesh].vao_id);

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	glDrawArrays(GL_TRIANGLES, 0, meshes[current_mesh].vertex_count);

	glDisableVertexAttribArray(2);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void init_ssao(int width, int height) {
	stages[1].shader = rafgl_program_create_from_name("ssao");
	stages[1].uni[0] = glGetUniformLocation(stages[1].shader, "uni_m");
	stages[1].uni[1] = glGetUniformLocation(stages[1].shader, "uni_v");
	stages[1].uni[2] = glGetUniformLocation(stages[1].shader, "uni_p");
	stages[1].uni[3] = glGetUniformLocation(stages[1].shader, "size_kernel");
	stages[1].fbo = rafgl_framebuffer_simple_create(width, height);

	glBindFramebuffer(GL_FRAMEBUFFER, stages[1].fbo.fbo_id);

	smp_position = glGetUniformLocation(stages[1].shader, "smp_position");
	smp_normal = glGetUniformLocation(stages[1].shader, "smp_normal");
	smp_noise = glGetUniformLocation(stages[1].shader, "smp_noise");
	smp_kernel = glGetUniformLocation(stages[1].shader, "smp_kernel");

	glUseProgram(stages[1].shader);
	glUniform1i(smp_position, 0);
	glUniform1i(smp_normal, 1);
	glUniform1i(smp_noise, 2);
	glUniform1i(smp_kernel, 3);

	glGenTextures(1, &tex_ssao);
	glBindTexture(GL_TEXTURE_2D, tex_ssao);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_ssao, 0);

	float noise[size_noise][3];

	for(int i = 0; i < size_noise; i++) {
		noise[i][0] = ((float) rand()) / RAND_MAX;
		noise[i][1] = ((float) rand()) / RAND_MAX;
		noise[i][2] = ((float) rand()) / RAND_MAX;
	}

	glGenTextures(1, &tex_noise);
	glBindTexture(GL_TEXTURE_2D, tex_noise);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, sqrt(size_noise), sqrt(size_noise), 0, GL_RGB, GL_FLOAT, noise);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	float kernel[size_kernel][3];

	for(int i = 0; i < size_kernel; i++) {
		kernel[i][0] = ((float) rand()) / RAND_MAX;
		kernel[i][1] = ((float) rand()) / RAND_MAX;
		kernel[i][2] = ((float) rand()) / RAND_MAX;
	}

	glGenTextures(1, &tex_kernel);
	glBindTexture(GL_TEXTURE_2D, tex_kernel);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, sqrt(size_kernel), sqrt(size_kernel), 0, GL_RGB, GL_FLOAT, kernel);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void render_ssao() {
	glBindFramebuffer(GL_FRAMEBUFFER, stages[1].fbo.fbo_id);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(stages[1].shader);
	glUniformMatrix4fv(stages[1].uni[0], 1, GL_FALSE, (void*) model.m);
	glUniformMatrix4fv(stages[1].uni[1], 1, GL_FALSE, (void*) view.m);
	glUniformMatrix4fv(stages[1].uni[2], 1, GL_FALSE, (void*) projection.m);
	glUniform1i(stages[1].uni[3], size_kernel);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex_position);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, tex_normal);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, tex_noise);

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, tex_kernel);

	render_quad();
}

void init_blur(int width, int height) {
	stages[2].shader = rafgl_program_create_from_name("blur");
	stages[2].uni[0] = glGetUniformLocation(stages[2].shader, "uni_m");
	stages[2].uni[1] = glGetUniformLocation(stages[2].shader, "uni_v");
	stages[2].uni[2] = glGetUniformLocation(stages[2].shader, "uni_p");
	stages[2].fbo = rafgl_framebuffer_simple_create(width, height);

	glBindFramebuffer(GL_FRAMEBUFFER, stages[2].fbo.fbo_id);

	smp_ssao = glGetUniformLocation(stages[2].shader, "smp_ssao");

	glUseProgram(stages[2].shader);
	glUniform1i(smp_ssao, 0);

	glGenTextures(1, &tex_blur);
	glBindTexture(GL_TEXTURE_2D, tex_blur);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RGB, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_blur, 0);
}

void render_blur() {
	glBindFramebuffer(GL_FRAMEBUFFER, stages[2].fbo.fbo_id);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(stages[2].shader);
	glUniformMatrix4fv(stages[2].uni[0], 1, GL_FALSE, (void*) model.m);
	glUniformMatrix4fv(stages[2].uni[1], 1, GL_FALSE, (void*) view.m);
	glUniformMatrix4fv(stages[2].uni[2], 1, GL_FALSE, (void*) projection.m);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex_ssao);

	render_quad();
}

void init_light(int width, int height) {
	stages[3].shader = rafgl_program_create_from_name("light");
	stages[3].uni[0] = glGetUniformLocation(stages[3].shader, "uni_m");
	stages[3].uni[1] = glGetUniformLocation(stages[3].shader, "uni_v");
	stages[3].uni[2] = glGetUniformLocation(stages[3].shader, "uni_p");
	stages[3].uni[3] = glGetUniformLocation(stages[3].shader, "flag_ssao");
	stages[3].fbo = rafgl_framebuffer_simple_create(width, height);

	glBindFramebuffer(GL_FRAMEBUFFER, stages[3].fbo.fbo_id);

	smp_position = glGetUniformLocation(stages[3].shader, "smp_position");
	smp_normal = glGetUniformLocation(stages[3].shader, "smp_normal");
	smp_color = glGetUniformLocation(stages[3].shader, "smp_color");
	smp_blur = glGetUniformLocation(stages[3].shader, "smp_blur");

	glUseProgram(stages[3].shader);
	glUniform1i(smp_position, 0);
	glUniform1i(smp_normal, 1);
	glUniform1i(smp_color, 2);
	glUniform1i(smp_blur, 3);

	glGenTextures(1, &tex_light);
	glBindTexture(GL_TEXTURE_2D, tex_light);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_light, 0);
}

void render_light() {
	glBindFramebuffer(GL_FRAMEBUFFER, stages[3].fbo.fbo_id);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(stages[3].shader);
	glUniformMatrix4fv(stages[3].uni[0], 1, GL_FALSE, (void*) model.m);
	glUniformMatrix4fv(stages[3].uni[1], 1, GL_FALSE, (void*) view.m);
	glUniformMatrix4fv(stages[3].uni[2], 1, GL_FALSE, (void*) projection.m);
	glUniform1f(stages[3].uni[3], flag_ssao);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex_position);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, tex_normal);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, tex_color);

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, tex_blur);

	render_quad();
}

void main_state_init(GLFWwindow* window, void* args, int width, int height) {
	srand(time(NULL));

	rafgl_log_fps(RAFGL_TRUE);

	for(int i = 0; i < 1; i++) {
		rafgl_meshPUN_init(meshes + i);
		rafgl_meshPUN_load_from_OBJ(meshes + i, mesh_names[i]);
	}

	init_geometry(width, height);
	init_ssao(width, height);
	init_blur(width, height);
	init_light(width, height);

	glEnable(GL_DEPTH_TEST);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void main_state_update(GLFWwindow* window, float delta_time, rafgl_game_data_t* game_data, void* args) {
	if(game_data->keys_down[RAFGL_KEY_UP]) {
		angle_v -= angle_speed;
	} else if(game_data->keys_down[RAFGL_KEY_DOWN]) {
		angle_v += angle_speed;
	}

	if(game_data->keys_down[RAFGL_KEY_LEFT]) {
		angle_h -= angle_speed;
	} else if(game_data->keys_down[RAFGL_KEY_RIGHT]) {
		angle_h += angle_speed;
	}

	aim_dir = v3_norm(vec3(cosf(angle_h), angle_v, sinf(angle_h)));
	vec3_t right = v3_cross(aim_dir, vec3(0.0f, 1.0f, 0.0f));

	if(game_data->keys_down['W']) {
		camera_dir = v3_add(camera_dir, v3_muls(aim_dir, speed_move * delta_time));
	} else if(game_data->keys_down['S']) {
		camera_dir = v3_add(camera_dir, v3_muls(aim_dir, -speed_move * delta_time));
	}

	if(game_data->keys_down['D']) {
		camera_dir = v3_add(camera_dir, v3_muls(right, speed_move * delta_time));
	} else if(game_data->keys_down['A']) {
		camera_dir = v3_add(camera_dir, v3_muls(right, -speed_move * delta_time));
	}

	if(game_data->keys_down[RAFGL_KEY_SPACE]) {
		camera_dir.y += 1.0f * delta_time * speed_move;
	} else if(game_data->keys_down[RAFGL_KEY_LEFT_SHIFT]) {
		camera_dir.y -= 1.0f * delta_time * speed_move;
	}

	if(game_data->keys_pressed['R']) {
		flag_rotate = !flag_rotate;
	}

	if(game_data->keys_pressed['O']) {
		flag_ssao = 1.0f - flag_ssao;
	}

	if(game_data->keys_down[RAFGL_KEY_ESCAPE]) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}

	int i;

	for(i = 0; i < 9; i++) {
		if(current_texture != i && game_data->keys_pressed[RAFGL_KEY_0 + i + 1]) {
			current_texture = i;
		}
	}

	model = m4_identity();
	model = m4_rotation_y(angle_model += delta_time * flag_rotate);
	model = m4_mul(model, m4_translation(vec3(0.0f, sinf(angle_model) * 0.5f, 0.0f)));
	model = m4_mul(model, m4_translation(vec3(0.0f, 0.8f, -3.2f)));
	model = m4_mul(model, m4_scaling(vec3(factor_scale, factor_scale, factor_scale)));

	view = m4_look_at(camera_dir, v3_add(camera_dir, aim_dir), camera_up);

	float raster_aspect = ((float) (game_data->raster_width)) / game_data->raster_height;
	projection = m4_perspective(FIELD_OF_VIEW, raster_aspect, 0.1f, 100.0f);
	view_projection = m4_mul(projection, view);
}

void main_state_render(GLFWwindow* window, void* args) {
	render_geometry();
	render_ssao();
	render_blur();
	render_light();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	rafgl_texture_t tex;

	switch(current_texture) {
		case 0:
			tex.tex_id = tex_position;
			break;
		case 1:
			tex.tex_id = tex_normal;
			break;
		case 2:
			tex.tex_id = tex_color;
			break;
		case 3:
			tex.tex_id = tex_noise;
			break;
		case 4:
			tex.tex_id = tex_kernel;
			break;
		case 5:
			tex.tex_id = tex_ssao;
			break;
		case 6:
			tex.tex_id = tex_blur;
			break;
		case 7:
			tex.tex_id = tex_light;
			break;
	}

	rafgl_texture_show(&tex, 1);
}

void main_state_cleanup(GLFWwindow* window, void* args) {
	int i;

	for(i = 0; i < TOTAL_STAGES; i++) {
		glDeleteShader((GLuint) stages[i].shader);
	}
}
