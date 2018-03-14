#include <ncurses.h>
#include <random>
#include <array>
#include <string>
#include <iostream>
#include <memory>
#include <chrono>

// local namespace
namespace {

enum AppStatus {
	MENU,
	GAME,
	INFO,
	EXIT
};

// In this style 'cause ncurse already has function 'UP'
enum Direction {
	Up,
	Right,
	Down,
	Left
};

} // local namespace

/*
 * 		RandomCoordinatesGenerator
 */
class RandomCoordinatesGenerator {
public:
	RandomCoordinatesGenerator(unsigned short width, unsigned short height) noexcept :
		_w_distribution{1, width}, _h_distribution{1, height}
	{
		std::random_device random_device;
		_random_generator = std::mt19937(random_device());
	}

	std::pair<unsigned short, unsigned short> get() noexcept {
		return {
			_w_distribution(_random_generator),
			_h_distribution(_random_generator)
		};
	};
private:
	std::mt19937 _random_generator;
	std::uniform_int_distribution<unsigned short> _w_distribution;
	std::uniform_int_distribution<unsigned short> _h_distribution;
};

/*
 * 		Snake
 */

class Snake {
public:
	Snake(unsigned short init_x, unsigned short init_y, Direction direction) :
		_head_x{init_x}, _head_y{init_y}, _direction{direction} {}

	void setDirection(Direction direction) noexcept {
		_direction = direction;
	}

	void render() noexcept {
		clear();
		switch (_direction) {
			case Up:
				mvprintw(_head_y--, _head_x, _body_fill);
				break;
			case Right:
				mvprintw(_head_y, _head_x++, _body_fill);
				break;
			case Down:
				mvprintw(_head_y++, _head_x, _body_fill);
				break;
			case Left:
				mvprintw(_head_y, _head_x--, _body_fill);
				break;
		}
	}

	std::pair<unsigned short, unsigned short> getHead() const noexcept {
		return {_head_x, _head_y};
	};

private:
	const char * _body_fill{"@"};
	unsigned short _head_x, _head_y;
	Direction _direction;
};

/*
 * 		Screen
 */
class Screen {
public:
	Screen(unsigned short & width, unsigned short & height) :
		_width{width}, _height{height} {}

	virtual ~Screen() = default;

	virtual void render() noexcept = 0;
	virtual void input_handler(int, AppStatus&) noexcept = 0;

protected:
	unsigned short get_width() const noexcept {
		return _width;
	}

	unsigned short get_height() const noexcept {
		return _height;
	}

	void clear_once() noexcept {
		if (!_was_cleaned) {
			clear();
			_was_cleaned = true;
		}
	}

	void on_leave() noexcept {
		_was_cleaned = false;
	}

private:
	unsigned short & _width;
	unsigned short & _height;
	bool _was_cleaned{false};
};

/*
 * 		Game
 */
class Game : public Screen {
public:
	Game(unsigned short &width, unsigned short &height) : Screen(width, height) {
		_coords_generator = new RandomCoordinatesGenerator(width, height);
		_snake = new Snake{10, 10, Right};

		generate_food();
	}

	~Game() override {
		delete _snake;
	}

	void render() noexcept override {
		clear_once();
		auto now = std::chrono::system_clock::now();
		if ( std::chrono::duration_cast<std::chrono::milliseconds>(now - _previous_render).count() > 100 ) {
			_previous_render = now;
			_snake->render();

			draw_score();
			draw_food();
			if (check_food()) {
				_score++;
				generate_food();
			}
		}
	}

	void input_handler(int input, AppStatus& status) noexcept override {
		switch (input) {
			case KEY_UP:
				_snake->setDirection(Up);
				break;
			case KEY_RIGHT:
				_snake->setDirection(Right);
				break;
			case KEY_DOWN:
				_snake->setDirection(Down);
				break;
			case KEY_LEFT:
				_snake->setDirection(Left);
				break;
			case 'q':
				on_leave();
				status = MENU;
				break;
			default:
				break;
		}
	}

private:
	void generate_food() {
		auto [pos_x, pos_y] = _coords_generator->get();
		_food = {pos_x, pos_y};
	}

	void draw_food() {
		mvprintw(_food.second, _food.first, "$");
	}

	bool check_food() {
		auto [head_x, head_y] = _snake->getHead();
		return (_food.first == head_x && _food.second == head_y);
	}

	void draw_score() {
		mvprintw(1, get_width() / 2, "score %d", _score);
	}

	unsigned short _score{0};
	std::chrono::system_clock::time_point _previous_render;
	std::pair<unsigned short, unsigned short> _food;
	RandomCoordinatesGenerator* _coords_generator;
	Snake* _snake;
};

/*	
 * 		MENU
 */
class Menu : public Screen {
public:
	Menu(unsigned short &width, unsigned short &height) : Screen(width, height) {}

	void render() noexcept override {
		clear_once();
		for (int i = 0; i < _menu_options.size(); ++i) {
			if (i == _current_option) {
				attron(COLOR_PAIR(1));
				mvprintw(get_height() / 2 + i, get_width() / 2, _menu_options[i].c_str());
				attroff(COLOR_PAIR(1));
			} else {
				mvprintw(get_height() / 2 + i, get_width() / 2, _menu_options[i].c_str());
			}
		}
	}

	void input_handler(int input, AppStatus& status) noexcept override {
		switch (input) {
			case KEY_UP:
				if (_current_option != 0) {
					_current_option--;
				} else {
					_current_option = _menu_options.size() - 1;
				}
				break;
			case KEY_DOWN:
				if (_current_option != _menu_options.size() - 1) {
					_current_option++;
				} else {
					_current_option = 0;
				}
				break;
			case '\n':
				on_leave();
				status = static_cast<AppStatus>(_current_option + 1);
			default:
				break;
		}
	}

private:
	std::array<std::string, 3> _menu_options{ {"Start", "Info", "Exit"} };
	unsigned short _current_option{0};
};

/*
 * 		Info
 */
class Info : public Screen {
public:
	Info(unsigned short &width, unsigned short &height) : Screen(width, height) {}

	void render() noexcept override {
		clear_once();
		mvprintw(get_height() / 2, get_width() / 2, "print q to back in menu");
	}

	void input_handler(int input, AppStatus& status) noexcept override {
		switch (input) {
			case 'q':
				on_leave();
				status = MENU;
				break;
			default:
				break;
		}
	}

};

class SnakeGame {
public:
	SnakeGame() {
		// Init screen
		initscr();
		// Get current size of terminal window
		getmaxyx(stdscr, _height, _width);

		_menu = new Menu(_width, _height);
		_info = new Info(_width, _height);
		_game = new Game(_width, _height);
	}

	~SnakeGame() {
		delete _game;
		delete _info;
		delete _menu;
	}

	void start() {
		init();
		render();
	}
private:

	void init() {
		// Disable cursor display
		curs_set(0);
		// Make available arrows
		keypad(stdscr, true);
		// Disable buffering
		noecho();
		// Input in no-blocking regime
		nodelay(stdscr, true);

		// Make available colors
		start_color();
		// Init own color scheme
		init_pair(1, COLOR_CYAN, COLOR_BLUE);
	}

	void render() {
		while (_status != EXIT) {
			box(stdscr, 0, 0);
			refresh();
			if ( (_input = getch()) == ERR ) {
				switch (_status) {
					case MENU:
						_menu->render();
						break;
					case INFO:
						_info->render();
						break;
					case GAME:
						_game->render();
					default:
						break;
				}
			} else {
				switch (_status) {
					case MENU:
						_menu->input_handler(_input, _status);
						break;
					case INFO:
						_info->input_handler(_input, _status);
						break;
					case GAME:
						_game->input_handler(_input, _status);
						break;
					default:
						break;
				}
			}
		}
		endwin();
	}

	Menu* _menu;
	Info* _info;
	Game* _game;

	AppStatus _status{MENU};
	unsigned short _height{}, _width{};
	int _input{};
};

int main() {
	auto game = std::make_unique<SnakeGame>();
	game->start();
	return 0;
}

