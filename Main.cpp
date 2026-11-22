#include<iostream>
#include<vector>
#include<cmath>
#include<string>
#include<algorithm>

#include<glad/glad.h>
#include<GLFW/glfw3.h>


#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_glfw.h"
#include "IMGUI/imgui_impl_opengl3.h"


const char* vertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec3 aPos;\n"
"layout (location = 1) in vec4 aColor;\n"
"out vec4 VertexColor;\n"
"void main()\n"
"{\n"
"   gl_Position = vec4(aPos, 1.0);\n"
"   VertexColor = aColor;\n"
"}\0";
const char* fragmentShaderSource = "#version 330 core\n"
"in vec4 VertexColor;\n"
"out vec4 FragColor;\n"
"void main()\n"
"{\n"
"   FragColor = VertexColor;\n"
"}\n\0";


struct VertexSquare {
	float pos[3];
	float color[4];
};

enum TipoFigura {FIG_LINEA, FIG_CIRCULO, FIG_RECTANGULO, FIG_TRIANGULO};

struct Figura {
	TipoFigura tipo;
	char nombre[32];
	float color[4];
	float colorRelleno[4];
	bool rellenado;

	int x0, y0, x1, y1, x2, y2; //Lina, cuadrado, triangulo
	int x, y, r; // Circulo

};

static GLuint VAO = 0, VBO = 0, EBO = 0;
static std::vector<VertexSquare> g_gridVerts;
static std::vector<GLuint> g_gridIndices;

static int g_rows = 100;
static int g_cols = 100;
static float g_baseColor[4] = { 0.0f, 1.0f, 1.0f, 1.0f }; // Color de la grilla
//static float g_lineColorLinea[4] = {1.0f, 0.0f, 0.0f, 1.0f}; // Color de la línea 
//static float g_lineColorCirculo[4] = { 1.0f, 0.0f, 1.0f, 1.0f }; // Color del círculo
//static int g_x0 = -39, g_y0 = -35, g_x1 = 2, g_y1 = 0;  // linea x=-49 a 39, y= -40 a 39 de ahi se sale del cuadrante de pantalla (800x800)

static std::vector<Figura> g_figuras;

/*static int g_centerX = 10;  // Centro X 
static int g_centerY = 10;  // Centro Y 
static int g_radius = 10;   // Radio*/

static int g_modoCreacion = -1;
static int g_pasoMouse = 0;
static int g_tempX0 = 0, g_tempY0 = 0;
static int g_tempX1 = 0, g_tempY1 = 0;

void generatePixelGrid(std::vector<VertexSquare>& outVerts, std::vector<GLuint>& outIndices,
	int rows, int cols, const float color[4])
{
	outVerts.clear();
	outIndices.clear();
	outVerts.reserve(static_cast<size_t>(cols) * rows * 4);
	outIndices.reserve(static_cast<size_t>(cols) * rows * 6);

	float cellWidth = 2.0f / cols;
	float cellHeight = 2.0f / rows;

	for (int r = 0; r < rows; ++r) {
		for (int c = 0; c < cols; ++c) {
			float cx = -1.0f + (c + 0.5f) * cellWidth;
			float cy = -1.0f + (r + 0.5f) * cellHeight;
			float halfW = cellWidth * 0.5f;
			float halfH = cellHeight * 0.5f;


			VertexSquare v0 = { { cx + halfW, cy + halfH, 0.0f }, { color[0], color[1], color[2], color[3] } }; // Sup_Der
			VertexSquare v1 = { { cx + halfW, cy - halfH, 0.0f }, { color[0], color[1], color[2], color[3] } }; // Inf_Der
			VertexSquare v2 = { { cx - halfW, cy - halfH, 0.0f }, { color[0], color[1], color[2], color[3] } }; // Inf_Izq
			VertexSquare v3 = { { cx - halfW, cy + halfH, 0.0f }, { color[0], color[1], color[2], color[3] } }; // Sup_Izq

			GLuint base = static_cast<GLuint>(outVerts.size());
			outVerts.push_back(v0);
			outVerts.push_back(v1);
			outVerts.push_back(v2);
			outVerts.push_back(v3);


			outIndices.push_back(base + 0);
			outIndices.push_back(base + 1);
			outIndices.push_back(base + 2);
			outIndices.push_back(base + 0);
			outIndices.push_back(base + 2);
			outIndices.push_back(base + 3);
		}
	}
}

//Bresenham

void setCellColor(std::vector<VertexSquare>& verts, int cols, int rows, int cx, int cy, const float color[4]) {
	if (cx < 0 || cx >= cols || cy < 0 || cy >= rows) return;
	size_t cellIndex = static_cast<size_t>(cy) * cols + static_cast<size_t>(cx);
	size_t base = cellIndex * 4;
	for (size_t i = 0; i < 4; ++i) {
		verts[base + i].color[0] = color[0];
		verts[base + i].color[1] = color[1];
		verts[base + i].color[2] = color[2];
		verts[base + i].color[3] = color[3];
	}
};

//linea de bresenham
void LineBresenhamH(std::vector<VertexSquare>& verts, int cols, int rows,
	int x0, int y0, int x1, int y1, const float color[4])
{
	if (x0 > x1) {
		std::swap(x0, x1);
		std::swap(y0, y1);
	}

	int dx = x1 - x0;
	int dy = y1 - y0;

	int dir = (dy < 0) ? -1 : 1;
	dy *= dir;

	if (dx != 0) {
		int y = y0;
		int p = 2 * dy - dx;

		for (int i = 0; i <= dx; ++i) {
			setCellColor(verts, cols, rows, x0 + i, y, color);
			if (p >= 0) {
				y += dir;
				p -= 2 * dx;
			}
			p += 2 * dy;
		}
	}
}

void LineBresenhamV(std::vector<VertexSquare>& verts, int cols, int rows,
	int x0, int y0, int x1, int y1, const float color[4])
{
	if (y0 > y1) {
		std::swap(x0, x1);
		std::swap(y0, y1);
	}

	int dx = x1 - x0;
	int dy = y1 - y0;

	int dir = (dx < 0) ? -1 : 1;
	dx *= dir;

	if (dy != 0) {
		int x = x0;
		int p = 2 * dx - dy;

		for (int i = 0; i <= dy; ++i) {
			setCellColor(verts, cols, rows, x, y0 + i, color);
			if (p >= 0) {
				x += dir;
				p -= 2 * dy;
			}
			p += 2 * dx;
		}
	}
}

void LineBresenham(std::vector<VertexSquare>& verts, int cols, int rows,
	int x0, int y0, int x1, int y1, const float color[4])
{
	int cx0 = x0 + cols / 2;
	int cy0 = y0 + rows / 2;
	int cx1 = x1 + cols / 2;
	int cy1 = y1 + rows / 2;

	if (std::abs(cy1 - cy0) < std::abs(cx1 - cx0)) {
		LineBresenhamH(verts, cols, rows, cx0, cy0, cx1, cy1, color);
	}
	else {
		LineBresenhamV(verts, cols, rows, cx0, cy0, cx1, cy1, color);
	}
}

//Circulo de bresenham

void set8Octants(std::vector<VertexSquare>& verts, int cols, int rows,
	int cx, int cy, int x, int y, const float color[4])
{
	setCellColor(verts, cols, rows, cx + x, cy + y, color); // Octante 1
	setCellColor(verts, cols, rows, cx - x, cy + y, color); // Octante 2
	setCellColor(verts, cols, rows, cx + x, cy - y, color); // Octante 3
	setCellColor(verts, cols, rows, cx - x, cy - y, color); // Octante 4
	setCellColor(verts, cols, rows, cx + y, cy + x, color); // Octante 5
	setCellColor(verts, cols, rows, cx - y, cy + x, color); // Octante 6
	setCellColor(verts, cols, rows, cx + y, cy - x, color); // Octante 7
	setCellColor(verts, cols, rows, cx - y, cy - x, color); // Octante 8
}


void CirculoBresenham(std::vector<VertexSquare>& verts, int cols, int rows,
	int centerX, int centerY, int radius, const float colorBorde[4], 
	const float colorRelleno[4], bool rellenado)
{

	int cx = centerX + cols / 2;
	int cy = centerY + rows / 2;

	int x = 0;
	int y = -radius;
	int p = -radius;

	while (x < -y) {
		if (p > 0) {
			y += 1;
			p += 2 * (x + y) + 1;
		}
		else {
			p += 2 * x + 1;
		}

		set8Octants(verts, cols, rows, cx, cy, x, y, colorBorde);
		x += 1;
	}
}

//Rectangulo
void Rectangulo(std::vector<VertexSquare>& verts, int cols, int rows, int x0, int y0, int x1, int y1, 
	 const float colorBorde[4], const float colorRelleno[4], bool rellenado) {
	int cx0 = x0 + cols / 2;
	int cy0 = y0 + rows / 2;
	int cx1 = x1 + cols / 2;
	int cy1 = y1 + rows / 2;

	int minX = std::min(cx0, cx1);
	int maxX = std::max(cx0, cx1);
	int minY = std::min(cy0, cy1);
	int maxY = std::max(cy0, cy1);

	if (rellenado) {
		for (int y = minY + 1; y < maxY; ++y) {
			for (int x = minX + 1; x < maxX; ++x) {
				setCellColor(verts, cols, rows, x, y, colorRelleno);
			}
		}
	}

	LineBresenham(verts, cols, rows, x0, y0, x1, y0, colorBorde);
	LineBresenham(verts, cols, rows, x0, y1, x1, y1, colorBorde);
	LineBresenham(verts, cols, rows, x0, y0, x0, y1, colorBorde);
	LineBresenham(verts, cols, rows, x1, y0, x1, y1, colorBorde);
}

//Triangulo
void Triangulo(std::vector<VertexSquare>& verts, int cols, int rows, int x0, int y0, int x1, int y1, 
	 int x2, int y2, const float colorBorde[4], const float colorRelleno[4], bool rellenado) {
	int cx0 = x0 + cols / 2, cy0 = y0 + rows / 2;
	int cx1 = x1 + cols / 2, cy1 = y1 + rows / 2;
	int cx2 = x2 + cols / 2, cy2 = y2 + rows / 2;

	if (rellenado) {
		int minX = std::min({ cx0, cx1, cx2 });
		int maxX = std::max({ cx0, cx1, cx2 });
		int minY = std::min({ cy0, cy1, cy2 });
		int maxY = std::max({ cy0, cy1, cy2 });

		auto sign = [](int p1x, int p1y, int p2x, int p2y, int p3x, int p3y) {
			return (p1x - p3x) * (p2y - p3y) - (p2x - p3x) * (p1y - p3y);
			};

		for (int y = minY; y <= maxY; ++y) {
			for (int x = minX; x <= maxX; ++x) {
				float d1 = sign(x, y, cx0, cy0, cx1, cy1);
				float d2 = sign(x, y, cx1, cy1, cx2, cy2);
				float d3 = sign(x, y, cx2, cy2, cx0, cy0);

				bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
				bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);

				if (!(has_neg && has_pos)) {
					setCellColor(verts, cols, rows, x, y, colorRelleno);
				}
			}
		}
	}

	LineBresenham(verts, cols, rows, x0, y0, x1, y1, colorBorde);
	LineBresenham(verts, cols, rows, x1, y1, x2, y2, colorBorde);
	LineBresenham(verts, cols, rows, x2, y2, x0, y0, colorBorde);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	if (ImGui::GetIO().WantCaptureMouse) return;

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);

		int width, height;
		glfwGetWindowSize(window, &width, &height);

		float normalizedX = (static_cast<float>(xpos) / width) * 2.0f - 1.0f;
		float normalizedY = 1.0f - (static_cast<float>(ypos) / height) * 2.0f;

		int gridC = static_cast<int>((normalizedX + 1.0f) * (g_cols / 2.0f));
		int gridR = static_cast<int>((normalizedY + 1.0f) * (g_rows / 2.0f));

		int gridX = gridC - g_cols / 2;
		int gridY = gridR - g_rows / 2;

		if (g_modoCreacion == 0) { // Línea 
			if (g_pasoMouse == 0) {
				g_tempX0 = gridX; g_tempY0 = gridY;
				g_pasoMouse = 1;
			} else {
				Figura nuevaLinea;
				nuevaLinea.tipo = FIG_LINEA;
				snprintf(nuevaLinea.nombre, sizeof(nuevaLinea.nombre), "Linea %zu", g_figuras.size() + 1);
				nuevaLinea.color[0] = 0.0f; nuevaLinea.color[1] = 1.0f; nuevaLinea.color[2] = 0.0f; nuevaLinea.color[3] = 1.0f;
				nuevaLinea.x0 = g_tempX0; nuevaLinea.y0 = g_tempY0;
				nuevaLinea.x1 = gridX; nuevaLinea.y1 = gridY;
				nuevaLinea.rellenado = false;
				g_figuras.push_back(nuevaLinea);
				g_pasoMouse = 0; g_modoCreacion = -1;
			}
		} else if (g_modoCreacion == 1) { // Círculo 
			if (g_pasoMouse == 0) {
				g_tempX0 = gridX; g_tempY0 = gridY;
				g_pasoMouse = 1;
			} else {
				int radio = static_cast<int>(std::sqrt(std::pow(gridX - g_tempX0, 2) + std::pow(gridY - g_tempY0, 2)));
				if (radio < 1) radio = 1;

				Figura nuevoCirculo;
				nuevoCirculo.tipo = FIG_CIRCULO;
				snprintf(nuevoCirculo.nombre, sizeof(nuevoCirculo.nombre), "Circulo %zu", g_figuras.size() + 1);
				nuevoCirculo.color[0] = 1.0f; nuevoCirculo.color[1] = 0.0f; nuevoCirculo.color[2] = 1.0f; nuevoCirculo.color[3] = 1.0f;
				nuevoCirculo.colorRelleno[0] = 1.0f; nuevoCirculo.colorRelleno[1] = 1.0f; nuevoCirculo.colorRelleno[2] = 0.0f; nuevoCirculo.colorRelleno[3] = 1.0f;
				nuevoCirculo.x = g_tempX0; nuevoCirculo.y = g_tempY0;
				nuevoCirculo.r = radio;
				nuevoCirculo.rellenado = true;
				g_figuras.push_back(nuevoCirculo);
				g_pasoMouse = 0; g_modoCreacion = -1;
			}
		} else if (g_modoCreacion == 2) { // Rectángulo 
			if (g_pasoMouse == 0) {
				g_tempX0 = gridX; g_tempY0 = gridY;
				g_pasoMouse = 1;
			} else {
				Figura nuevoRect;
				nuevoRect.tipo = FIG_RECTANGULO;
				snprintf(nuevoRect.nombre, sizeof(nuevoRect.nombre), "Rectangulo %zu", g_figuras.size() + 1);
				nuevoRect.color[0] = 0.0f; nuevoRect.color[1] = 0.5f; nuevoRect.color[2] = 1.0f; nuevoRect.color[3] = 1.0f;
				nuevoRect.colorRelleno[0] = 0.2f; nuevoRect.colorRelleno[1] = 0.8f; nuevoRect.colorRelleno[2] = 0.8f; nuevoRect.colorRelleno[3] = 1.0f;
				nuevoRect.x0 = g_tempX0; nuevoRect.y0 = g_tempY0;
				nuevoRect.x1 = gridX; nuevoRect.y1 = gridY;
				nuevoRect.rellenado = true;
				g_figuras.push_back(nuevoRect);
				g_pasoMouse = 0; g_modoCreacion = -1;
			}
		}
		else if (g_modoCreacion == 3) { //Triangulo
			if (g_pasoMouse == 0) {
				g_tempX0 = gridX; g_tempY0 = gridY;
				g_pasoMouse = 1;
			}
			else if (g_pasoMouse == 1) {
				g_tempX1 = gridX; g_tempY1 = gridY;
				g_pasoMouse = 2;
			}
			else {
				Figura nuevoTri;
				nuevoTri.tipo = FIG_TRIANGULO;
				snprintf(nuevoTri.nombre, sizeof(nuevoTri.nombre), "Triangulo %zu", g_figuras.size() + 1);
				nuevoTri.color[0] = 1.0f; nuevoTri.color[1] = 0.5f; nuevoTri.color[2] = 0.0f; nuevoTri.color[3] = 1.0f; 
				nuevoTri.colorRelleno[0] = 1.0f; nuevoTri.colorRelleno[1] = 0.2f; nuevoTri.colorRelleno[2] = 0.2f; nuevoTri.colorRelleno[3] = 1.0f; 
				nuevoTri.x0 = g_tempX0; nuevoTri.y0 = g_tempY0;
				nuevoTri.x1 = g_tempX1; nuevoTri.y1 = g_tempY1;
				nuevoTri.x2 = gridX; nuevoTri.y2 = gridY;
				nuevoTri.rellenado = true;
				g_figuras.push_back(nuevoTri);
				g_pasoMouse = 0; g_modoCreacion = -1;
			}
		}
	}
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);

}

void shaders() {

}



int main()
{
	glfwInit();

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow* window = glfwCreateWindow(800, 800, "Ventana", NULL, NULL);
	if (window == NULL) {
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	
	gladLoadGL();
	glViewport(0, 0, 800, 800);


	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");


	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);

	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);


	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);


	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(window, true);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Gestor de Figuras Geometricas");
		ImGui::Text("FPS: %.1f", io.Framerate);
		ImGui::Separator();

		if (ImGui::Button("Linea")) {
			g_modoCreacion = 0; g_pasoMouse = 0;
		}
		if (ImGui::Button("Circulo")) {
			g_modoCreacion = 1; g_pasoMouse = 0;
		}
		if (ImGui::Button("Rectangulo")) {
			g_modoCreacion = 2; g_pasoMouse = 0;
		}
		if (ImGui::Button("Triangulo")) {
			g_modoCreacion = 3; g_pasoMouse = 0;
		}

		if (g_modoCreacion != -1) {
			ImGui::TextColored(ImVec4(0, 1, 0, 1), "Modo activo: Haz clic en la grilla.");
			if (g_modoCreacion == 3) {
				ImGui::Text("Paso actual: %d/3 clics dados.", g_pasoMouse + 1);
			}
			else if (g_pasoMouse > 0) {
				ImGui::Text("Primer punto fijado. Haz clic para el siguiente.");
			}
		}

		ImGui::Separator();
		ImGui::Text("Figuras Activas (%zu):", g_figuras.size());

		int indexParaBorrar = -1;
		for (size_t i = 0; i < g_figuras.size(); ++i) {
			ImGui::PushID((int)i);
			if (ImGui::TreeNode(g_figuras[i].nombre)) {
				if (g_figuras[i].tipo == FIG_LINEA) {
					ImGui::ColorEdit3("Color Linea", g_figuras[i].color);
					ImGui::SliderInt("X0", &g_figuras[i].x0, -50, 50);
					ImGui::SliderInt("Y0", &g_figuras[i].y0, -50, 50);
					ImGui::SliderInt("X1", &g_figuras[i].x1, -50, 50);
					ImGui::SliderInt("Y1", &g_figuras[i].y1, -50, 50);
				} else if (g_figuras[i].tipo == FIG_CIRCULO) {
					ImGui::ColorEdit3("Color Borde", g_figuras[i].color);
					ImGui::Checkbox("Rellenar Figura", &g_figuras[i].rellenado);
					if (g_figuras[i].rellenado) {
						ImGui::ColorEdit3("Color Relleno", g_figuras[i].colorRelleno);
					}
					ImGui::SliderInt("Centro X", &g_figuras[i].x, -40, 40);
					ImGui::SliderInt("Centro Y", &g_figuras[i].y, -40, 40);
					ImGui::SliderInt("Radio", &g_figuras[i].r, 1, 40);
				} else if (g_figuras[i].tipo == FIG_RECTANGULO) {
					ImGui::ColorEdit3("Color Borde", g_figuras[i].color);
					ImGui::Checkbox("Rellenar Figura", &g_figuras[i].rellenado);
					if (g_figuras[i].rellenado) {
						ImGui::ColorEdit3("Color Relleno", g_figuras[i].colorRelleno);
					}
					ImGui::SliderInt("X0", &g_figuras[i].x0, -50, 50);
					ImGui::SliderInt("Y0", &g_figuras[i].y0, -50, 50);
					ImGui::SliderInt("X1", &g_figuras[i].x1, -50, 50);
					ImGui::SliderInt("Y1", &g_figuras[i].y1, -50, 50);
				} else if (g_figuras[i].tipo == FIG_TRIANGULO) {
					ImGui::ColorEdit3("Color Borde", g_figuras[i].color);
					ImGui::Checkbox("Rellenar Figura", &g_figuras[i].rellenado);
					if (g_figuras[i].rellenado) {
						ImGui::ColorEdit3("Color Relleno", g_figuras[i].colorRelleno);
					}
					ImGui::SliderInt("X0", &g_figuras[i].x0, -50, 50);
					ImGui::SliderInt("Y0", &g_figuras[i].y0, -50, 50);
					ImGui::SliderInt("X1", &g_figuras[i].x1, -50, 50);
					ImGui::SliderInt("Y1", &g_figuras[i].y1, -50, 50);
					ImGui::SliderInt("X2", &g_figuras[i].x2, -50, 50);
					ImGui::SliderInt("Y2", &g_figuras[i].y2, -50, 50);
				}

				if (ImGui::Button("Eliminar Figura")) {
					indexParaBorrar = (int)i;
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		if (indexParaBorrar != -1) {
			g_figuras.erase(g_figuras.begin() + indexParaBorrar);
		}

		ImGui::End();

		ImGui::Render();

		generatePixelGrid(g_gridVerts, g_gridIndices, g_rows, g_cols, g_baseColor);

		for (const auto& fig : g_figuras) {
			if (fig.tipo == FIG_LINEA) {
				LineBresenham(g_gridVerts, g_cols, g_rows, fig.x0, fig.y0, fig.x1, fig.y1, fig.color);
			} else if (fig.tipo == FIG_CIRCULO) {
				CirculoBresenham(g_gridVerts, g_cols, g_rows, fig.x, fig.y, fig.r, fig.color, fig.colorRelleno, fig.rellenado);
			} else if (fig.tipo == FIG_RECTANGULO) {
				Rectangulo(g_gridVerts, g_cols, g_rows, fig.x0, fig.y0, fig.x1, fig.y1, fig.color, fig.colorRelleno, fig.rellenado);
			} else if (fig.tipo == FIG_TRIANGULO) {
				Triangulo(g_gridVerts, g_cols, g_rows, fig.x0, fig.y0, fig.x1, fig.y1, fig.x2, fig.y2, fig.color, fig.colorRelleno, fig.rellenado);
			}
		}


	    glBindVertexArray(VAO);

	    glBindBuffer(GL_ARRAY_BUFFER, VBO);
	    glBufferData(GL_ARRAY_BUFFER, g_gridVerts.size() * sizeof(VertexSquare), g_gridVerts.data(), GL_STATIC_DRAW);

	    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	    glBufferData(GL_ELEMENT_ARRAY_BUFFER, g_gridIndices.size() * sizeof(GLuint), g_gridIndices.data(), GL_STATIC_DRAW);

	    GLsizei stride = sizeof(VertexSquare);

	    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
	    glEnableVertexAttribArray(0);

	    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
	    glEnableVertexAttribArray(1);

	    glBindVertexArray(0);

    	glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
	    glClear(GL_COLOR_BUFFER_BIT);

    	glUseProgram(shaderProgram);
	    glBindVertexArray(VAO);
	    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(g_gridIndices.size()), GL_UNSIGNED_INT, 0);

    	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    	glfwSwapBuffers(window);

    }


    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &EBO);
	glDeleteProgram(shaderProgram);

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}