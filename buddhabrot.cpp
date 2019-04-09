#include <glad/glad.h>
#include <glfw3.h>
#include <math.h>
#include <vector>
#include <stack>
#include <iostream>
#include <fstream>
#include <chrono>
#include <random>
#include <sstream>
typedef float HeatmapType;
using namespace std;

vector<float> vertices;
// settings
const int IMAGE_HEIGHT = 200;
const int IMAGE_WIDTH = 200;
const int RED_ITERS = 200;
const int BLUE_ITERS = 800;
const int GREEN_ITERS = 200;
const long long int SAMPLE_COUNT = IMAGE_WIDTH * IMAGE_HEIGHT * 100;

class Complex
{
  public:
	Complex(double r = 0.0, double i = 0.0)
		: _r(r), _i(i)
	{
	}

	Complex(const Complex &) = default;

	double r() const { return _r; }
	double i() const { return _i; }

	Complex operator*(const Complex &other)
	{
		// (a + bi) (c + di)
		return Complex(_r * other._r - _i * other._i, _r * other._i + _i * other._r);
	}

	Complex operator+(const Complex &other)
	{
		return Complex(_r + other._r, _i + other._i);
	}

	double sqmagnitude() const
	{
		return _r * _r + _i * _i;
	}

  private:
	double _r, _i;
};

//
// Utility
//
void AllocHeatmap(HeatmapType **&o_heatmap, int width, int height)
{
	o_heatmap = new HeatmapType *[height];
	for (int i = 0; i < height; ++i)
	{
		o_heatmap[i] = new HeatmapType[width];
		for (int j = 0; j < width; ++j)
		{
			o_heatmap[i][j] = 0;
		}
	}
}

void FreeHeatmap(HeatmapType **&o_heatmap, int height)
{
	for (int i = 0; i < height; ++i)
	{
		delete[] o_heatmap[i];
		o_heatmap[i] = nullptr;
	}
	delete o_heatmap;
	o_heatmap = nullptr;
}

vector<Complex> buddhabrotPoints(const Complex &c, int nIterations)
{
	int n = 0;
	Complex z;

	vector<Complex> toReturn;
	toReturn.reserve(nIterations);

	while (n < nIterations && z.sqmagnitude() <= 2.0)
	{
		z = z * z + c;
		++n;

		toReturn.push_back(z);
	}

	// If point remains bounded through nIterations iterations, the point
	//  is bounded, therefore in the Mandelbrot set, therefore of no interest to us
	if (n == nIterations)
	{
		return vector<Complex>();
	}
	else
	{
		return toReturn;
	}
}

int rowFromReal(double real, double minR, double maxR, int imageHeight)
{
	// [minR, maxR]
	// [0, maxR - minR] // subtract minR from n
	// [0, imageHeight] // multiply by (imageHeight / (maxR - minR))
	return (int)((real - minR) * (imageHeight / (maxR - minR)));
}

int colFromImaginary(double imag, double minI, double maxI, int imageWidth)
{
	return (int)((imag - minI) * (imageWidth / (maxI - minI)));
}

void GenerateHeatmap(HeatmapType **o_heatmap, int imageWidth, int imageHeight,
					 const Complex &minimum, const Complex &maximum, int nIterations, long long nSamples,
					 HeatmapType &o_maxHeatmapValue, string consoleMessagePrefix)
{
	mt19937 rng;
	uniform_real_distribution<double> realDistribution(minimum.r(), maximum.r());
	uniform_real_distribution<double> imagDistribution(minimum.i(), maximum.i());

	rng.seed(chrono::high_resolution_clock::now().time_since_epoch().count());
	// Collect nSamples samples... (sample is just a random number c)
	for (long long sampleIdx = 0; sampleIdx < nSamples; ++sampleIdx)
	{
		//  Each sample, get the list of points as the function
		//    escapes to infinity (if it does at all)

		Complex sample(realDistribution(rng), imagDistribution(rng));
		vector<Complex> pointsList = buddhabrotPoints(sample, nIterations);

		for (Complex &point : pointsList)
		{
			if (point.r() <= maximum.r() && point.r() >= minimum.r() && point.i() <= maximum.i() && point.i() >= minimum.i())
			{
				int row = rowFromReal(point.r(), minimum.r(), maximum.r(), imageHeight);
				int col = colFromImaginary(point.i(), minimum.i(), maximum.i(), imageWidth);
				++o_heatmap[row][col];

				if (o_heatmap[row][col] > o_maxHeatmapValue)
				{
					o_maxHeatmapValue = o_heatmap[row][col];
				}
			}
		}
	}
}

float colorFromHeatmap(HeatmapType inputValue, HeatmapType maxHeatmapValue, float maxColor)
{
	double scale = (1.0f * (maxColor)) / (1.0f * maxHeatmapValue);
	return inputValue * scale;
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);
void processInput(GLFWwindow *window);

const char *vertexShaderSource = "#version 330 core\n"
								 "layout (location = 0) in vec3 aPos;\n"
								 "layout (location = 1) in vec3 aColor;\n"
								 "out vec3 vertexColor;\n"
								 "void main()\n"
								 "{\n"
								 "   gl_Position = vec4(aPos,1.0);\n"
								 "vertexColor = aColor;\n"
								 "}\0";
const char *fragmentShaderSource = "#version 330 core\n"
								   "out vec4 FragColor;\n"
								   "in vec3 vertexColor;\n"
								   "void main()\n"
								   "{\n"
								   "   FragColor =  vec4(vertexColor, 1.0);\n"
								   "}\n\0";

int points;

float mapX(float x)
{
	return x * 2.0 / IMAGE_WIDTH - 1.0;
}

float mapY(float y)
{
	return y * 2.0 / IMAGE_HEIGHT - 1.0;
}

int main()
{
	const Complex MINIMUM(-2.0, -2.0);
	const Complex MAXIMUM(2.0, 2.0);
	vector<float> vertices;
	vertices.reserve(IMAGE_HEIGHT * IMAGE_WIDTH);

	// Allocate a heatmap of the size of our image
	HeatmapType maxHeatmapValue = 0;
	HeatmapType **red;
	HeatmapType **green;
	HeatmapType **blue;
	AllocHeatmap(red, IMAGE_WIDTH, IMAGE_HEIGHT);
	AllocHeatmap(green, IMAGE_WIDTH, IMAGE_HEIGHT);
	AllocHeatmap(blue, IMAGE_WIDTH, IMAGE_HEIGHT);

	GenerateHeatmap(red, IMAGE_WIDTH, IMAGE_HEIGHT, MINIMUM, MAXIMUM, RED_ITERS,
					SAMPLE_COUNT, maxHeatmapValue, "Red Channel: ");
	GenerateHeatmap(blue, IMAGE_WIDTH, IMAGE_HEIGHT, MINIMUM, MAXIMUM, BLUE_ITERS,
					SAMPLE_COUNT, maxHeatmapValue, "Blue Channel: ");

	GenerateHeatmap(green, IMAGE_WIDTH, IMAGE_HEIGHT, MINIMUM, MAXIMUM, GREEN_ITERS,
					SAMPLE_COUNT, maxHeatmapValue, "green Channel: ");

	// Scale the heatmap down
	for (int row = 0; row < IMAGE_HEIGHT; ++row)
	{
		for (int col = 0; col < IMAGE_WIDTH; ++col)
		{

			red[row][col] = colorFromHeatmap(red[row][col], maxHeatmapValue, 1);
			blue[row][col] = colorFromHeatmap(blue[row][col], maxHeatmapValue, 1);
			green[row][col] = colorFromHeatmap(green[row][col], maxHeatmapValue, 1);

			//cout<<red[row][col]<<endl;
			vertices.push_back(-mapX(col));
			vertices.push_back(-mapY(row));
			vertices.push_back(0.0f);
			vertices.push_back(red[row][col]);
			vertices.push_back(green[row][col]);
			vertices.push_back(blue[row][col]);

			//	cout<<red[row][col]<<endl;
			//vertices.push_back(0.0f);
			//vertices.push_back(0.0f);
		}
	}

	// glfw: initialize and configure
	// ------------------------------
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // uncomment this statement to fix compilation on OS X
#endif

	// glfw window creation
	// --------------------
	GLFWwindow *window = glfwCreateWindow(IMAGE_WIDTH, IMAGE_HEIGHT, "Algorithmic Modeling Fractal Buddhabrot Set", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	//    glfwSetMouseButtonCallback(window, mouse_button_callback);

	// glad: load all OpenGL function pointers
	// ---------------------------------------
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	// build and compile our shader program
	// ------------------------------------
	// vertex shader
	int vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);
	// check for shader compile errors
	int success;
	char infoLog[512];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
				  << infoLog << std::endl;
	}
	// fragment shader
	int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);
	// check for shader compile errors
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n"
				  << infoLog << std::endl;
	}
	// link shaders
	int shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);
	// check for linking errors
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
				  << infoLog << std::endl;
	}
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	// set up vertex data (and buffer(s)) and configure vertex attributes
	// ------------------------------------------------------------------

	points = vertices.size();

	// uncomment this call to draw in wireframe polygons.
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	// render loop
	cout << "done" << endl;
	unsigned int VBO, VAO;

	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	// bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertices.size(), &vertices[0], GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	// note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
	// VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
	glBindVertexArray(0);
	// -----------
	while (!glfwWindowShouldClose(window))
	{

		processInput(window);

		//glClearColor(1,1, 1, 1.0f);
		//glClear(GL_COLOR_BUFFER_BIT);

		// draw our first triangle
		glUseProgram(shaderProgram);
		glBindVertexArray(VAO); // seeing as we only have a single VAO there's no need to bind it every time, but we'll do so to keep things a bit more organized
		glDrawArrays(GL_POINTS, 0, points);
		glfwSwapBuffers(window);
		glfwPollEvents();
		// ------------------------------------------------------------------
	}

	//glDeleteVertexArrays(1, &VAO);
	//glDeleteBuffers(1, &VBO);
	// optional: de-allocate all resources once they've outlived their purpose:
	// ------------------------------------------------------------------------

	// glfw: terminate, clearing all previously allocated GLFW resources.

	glfwTerminate();
	return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);
}
