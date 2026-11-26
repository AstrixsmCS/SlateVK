#include "SlateApp.hpp"

#include <Slate/Slate.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <SDL3/SDL.h>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <vector>

constexpr const char* vertexShader = R"(
	struct VertexInput
	{
		[[vk::location(0)]] float3 position : POSITION;
		[[vk::location(1)]] float3 color : COLOR0;
	};

	struct VertexOutput
	{
		float4 position : SV_Position;
		float3 color : COLOR0;
	};

	[[vk::push_constant]]
	ConstantBuffer<float4x4> mvp;

	[shader("vertex")]
	VertexOutput vertexMain(VertexInput input)
	{
		VertexOutput output;
		output.position = mul(mvp, float4(input.position, 1.0));
		output.color = input.color;
		return output;
	}
)";

constexpr const char* fragmentShader = R"(
	[shader("fragment")]
	float4 fragmentMain(float3 color : COLOR0) : SV_Target
	{
		return float4(color, 1.0);
	}
)";

struct Vertex
{
	float Position[3];
	float Color[3];
};

constexpr Vertex vertices[]
{
	// Front (+Z)
	{.Position = {-0.5f, -0.5f,  0.5f}, .Color = {1.0f, 0.0f, 0.0f}},
	{.Position = { 0.5f, -0.5f,  0.5f}, .Color = {0.0f, 1.0f, 0.0f}},
	{.Position = { 0.5f,  0.5f,  0.5f}, .Color = {0.0f, 0.0f, 1.0f}},
	{.Position = {-0.5f,  0.5f,  0.5f}, .Color = {1.0f, 1.0f, 0.0f}},

	// Back (-Z)
	{.Position = {-0.5f, -0.5f, -0.5f}, .Color = {1.0f, 0.0f, 1.0f}},
	{.Position = { 0.5f, -0.5f, -0.5f}, .Color = {0.0f, 1.0f, 1.0f}},
	{.Position = { 0.5f,  0.5f, -0.5f}, .Color = {1.0f, 1.0f, 1.0f}},
	{.Position = {-0.5f,  0.5f, -0.5f}, .Color = {1.0f, 0.5f, 0.0f}}
};

constexpr uint32_t indices[]
{
	// Front (+Z)
	0, 1, 2,
	2, 3, 0,

	// Back (-Z)
	5, 4, 7,
	7, 6, 5,

	// Left (-X)
	4, 0, 3,
	3, 7, 4,

	// Right (+X)
	1, 5, 6,
	6, 2, 1,

	// Top (+Y)
	3, 2, 6,
	6, 7, 3,

	// Bottom (-Y)
	4, 5, 1,
	1, 0, 4
};

int main()
{
	SlateApp app({
		.Title = "SlateVK - Cube",
		.Width = 1600,
		.Height = 900,
		.Mode = WindowMode::Windowed,
		.Resizable = true
	});

	if (!app.IsRunning())
		return 1;

	app.CenterWindow();

	Slate::ContextConfig config
	{
		.Version = Slate::VulkanVersion::VulkanVersion_1_4,
		.EnableValidationGpu = false
	};

	auto context = Slate::CreateContext(app.GetNativeWindow(), config);

	if (!context)
		return -1;

	VkBuffer vertexBuffer = context->CreateBuffer(vertices, sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	VkBuffer indexBuffer = context->CreateBuffer(indices, sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

	if (vertexBuffer == VK_NULL_HANDLE || indexBuffer == VK_NULL_HANDLE)
		return -1;

	std::vector<VkVertexInputBindingDescription> binding
	{
		{
			.binding = 0,
			.stride = sizeof(Vertex),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
		}
	};

	std::vector<VkVertexInputAttributeDescription> attributes
	{
		{
			.location = 0,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, Position)
		},
		{
			.location = 1,
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,
			.offset = offsetof(Vertex, Color)
		}
	};

	Slate::GraphicsPipelineSpecification spec
	{
		.Binding = binding,
		.Attributes = attributes,
		.VertexShader = vertexShader,
		.FragmentShader = fragmentShader
	};

	if (!context->CreateGraphicsPipeline(spec))
		return -1;

	while (app.IsRunning())
	{
		app.ProcessEvents();

		if (!app.IsRunning())
			break;

		int width = 0;
		int height = 0;
		SDL_GetWindowSizeInPixels(app.GetNativeWindow(), &width, &height);

		if (width <= 0 || height <= 0)
			continue;

		float time = static_cast<float>(SDL_GetTicks()) / 1000.0f;

		glm::mat4 model(1.0f);
		model = glm::rotate(model, time * glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, time * glm::radians(25.0f), glm::vec3(1.0f, 0.0f, 0.0f));

		glm::mat4 view = glm::lookAtRH(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 projection = glm::perspectiveRH_ZO(glm::radians(60.0f), static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f);

		// We do the y flip in the Vulkan Viewport so every app consumes it.
		// projection[1][1] *= -1.0f;

		glm::mat4 mvp = projection * view * model;

		context->Render(glm::value_ptr(mvp), vertexBuffer, indexBuffer, std::size(indices));
	}

	return 0;
}
