#include <ncurses.h>
#include <random>
#include <array>
#include <string>
#include <iostream>
#include <memory>
#include <chrono>
#include <string_view>
#include <algorithm>

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

typedef std::pair<unsigned short, unsigned short> coordinates;	// first = x; second = y;

} // local namespace

/*
 * 		RandomCoordinatesGenerator
 */
class RandomCoordinatesGenerator {
public:
	RandomCoordinatesGenerator(unsigned short width, unsigned short height) noexcept :
		_w_distribution(1, width - 1), _h_distribution(1, height - 1)
	{
		std::random_device random_device;
		_random_generator = std::mt19937(random_device());
	}

	coordinates get() noexcept {
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
	Snake(unsigned short init_x, unsigned short init_y, Direction direction) : _direction{direction} {
		_body_parts.reserve(100);
		_body_parts.emplace_back(init_x, init_y);
	}

	void setDirection(Direction direction) noexcept {
		_direction = direction;
	}

	void render() noexcept {
		clear();
		move_body();
		draw_body();
	}

	coordinates getHead() const noexcept {
		return _body_parts[0];
	};

	void grow_up() {
		_will_be_grown = true;
	}

private:
	void draw_body() {
		std::for_each(_body_parts.begin(), _body_parts.end(),
			      [this](const std::pair<unsigned short, unsigned short>& part) {
				      mvprintw(part.second, part.first, _body_fill);
			      }
		);
	}

	void move_body() {
		coordinates last;
		if (_will_be_grown) {
			last = _body_parts.back();
		}
		for (auto it = _body_parts.rbegin(), end = _body_parts.rend() - 1; it != end; ++it) {
			*it = *(it + 1);
		}
		if (_will_be_grown) {
			_body_parts.emplace_back(last);
			_will_be_grown = false;
		}
		switch (_direction) {
			case Up:
				_body_parts[0].second--;
				break;
			case Right:
				_body_parts[0].first++;
				break;
			case Down:
				_body_parts[0].second++;
				break;
			case Left:
				_body_parts[0].first--;
				break;
			default:
				break;
		}
	}

	bool _will_be_grown{false};
	std::vector<coordinates> _body_parts;
	const char * _body_fill{"@"};
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

	void print_on_center(std::string_view msg) const noexcept {
		mvprintw(_height / 2, (_width / 2) - (msg.size() / 2), msg.data());
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
		if (!is_paused) {
			auto now = std::chrono::system_clock::now();
			if ( std::chrono::duration_cast<std::chrono::milliseconds>(now - _previous_render).count() > 100 ) {
				_previous_render = now;

				_snake->render();

				draw_food_trace();
				draw_score();
				draw_food();

				if (check_food()) {
					_score++;
					_snake->grow_up();
					generate_food();
				}
			}
		} else {
			print_on_center("game was paused, press p to unpause");
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
			case 'p':
				is_paused = !is_paused;
				break;
			default:
				break;
		}
	}

private:
	void generate_food() {
		_food =  _coords_generator->get();
	}

	void draw_food() {
		mvprintw(_food.second, _food.first, "$");
	}

	bool check_food() {
		return _food == _snake->getHead();
	}

	void draw_score() {
		mvprintw(1, get_width() / 2, "score %d", _score);
	}

	void draw_food_trace() {
		mvprintw(1, 2, "x: %d", _food.first);
		mvprintw(2, 2, "y: %d", _food.second);
	}

	unsigned short _score{0};
	bool is_paused{false};
	std::chrono::system_clock::time_point _previous_render;
	coordinates _food;
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
		print_on_center("print q to back in menu");
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

