#include <random>
#include <tuple>
#include <ctime>
#include <SDL.h>
#include <entityx/entityx.h>
#include <entityx/deps/Dependencies.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <lfwatch.h>
#include "util.h"
#include "interleavedbuffer.h"
#include "events/input_event.h"
#include "systems/movement_system.h"
#include "systems/input_system.h"
#include "systems/asteroid_system.h"
#include "components/position.h"
#include "components/velocity.h"
#include "components/appearance.h"
#include "components/controllable.h"
#include "level.h"

Level::Level() : shader_program(0), viewing(), quit(false) {}
Level::~Level(){
	glDeleteProgram(shader_program);
}
void Level::receive(const InputEvent &input){
	switch (input.event.type){
		case SDL_KEYDOWN:
			quit = input.event.key.keysym.sym == SDLK_ESCAPE;
			break;
		default:
			break;
	}
}
bool Level::should_quit(){
	return quit;
}
void Level::configure(){
	system_manager->add<MovementSystem>();
	system_manager->add<AsteroidSystem>(10);
	system_manager->add<InputSystem>();
	system_manager->add<entityx::deps::Dependency<Asteroid, Position, Velocity>>();
	shader_program = util::load_program({std::make_tuple(GL_VERTEX_SHADER, "../res/vertex.glsl"),
		std::make_tuple(GL_FRAGMENT_SHADER, "../res/fragment.glsl")});
	viewing = InterleavedBuffer<Layout::PACKED, glm::mat4>{2, GL_UNIFORM_BUFFER, GL_STATIC_DRAW};
	assert(shader_program != -1);
	event_manager->subscribe<InputEvent>(*this);
	file_watcher.watch("../res", lfw::Notify::FILE_MODIFIED,
		[this](const lfw::EventData &e){
			if (e.fname == "vertex.glsl" || e.fname == "fragment.glsl"){
				this->load_shader();
			}
		});
}
void Level::initialize(){
	std::mt19937 mt_rand;
	mt_rand.seed(std::time(0));
	for (int i = 0; i < 10; ++i){
		entityx::Entity e = entity_manager->create();
		if (i == 0){
			e.assign<Controllable>();
		}
		else {
			e.assign<Position>(glm::vec2{static_cast<float>(mt_rand() % 8) - 4,
				static_cast<float>(mt_rand() % 8) - 4});
			e.assign<Velocity>(0.25f * glm::vec2{static_cast<float>(mt_rand() % 4) - 2,
				static_cast<float>(mt_rand() % 4) - 2});
		}
		e.assign<Asteroid>();
	}
	viewing.map(GL_WRITE_ONLY);
	viewing.write<0>(0) = glm::lookAt(glm::vec3{0.f, 0.f, 8.f}, glm::vec3{0.f, 0.f, 0.f},
		glm::vec3{0.f, 1.f, 0.f});
	viewing.write<0>(1) = glm::ortho(-5.f, 5.f, -5.f, 5.f, 1.f, 100.f);
	viewing.unmap();
	GLuint viewing_block = glGetUniformBlockIndex(shader_program, "Viewing");
	glUniformBlockBinding(shader_program, viewing_block, 0);
	viewing.bind_base(0);
	glUseProgram(shader_program);
}
void Level::update(double dt){
	file_watcher.update();
	system_manager->update<InputSystem>(dt);
	system_manager->update<MovementSystem>(dt);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	system_manager->update<AsteroidSystem>(dt);
}
void Level::load_shader(){
	GLint shader = util::load_program({std::make_tuple(GL_VERTEX_SHADER, "../res/vertex.glsl"),
			std::make_tuple(GL_FRAGMENT_SHADER, "../res/fragment.glsl")});
	if (shader == -1){
		std::cerr << "Error compiling reloaded shader, cancelling...\n";
	}
	else {
		glUseProgram(shader);
		glDeleteProgram(shader_program);
		shader_program = shader;
	}
}

