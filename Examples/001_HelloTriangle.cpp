#include "SlateApp.hpp"

#include <Slate/Slate.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <SDL3/SDL.h>

constexpr const char* vertexShader = R"(
	struct VertexOutput
	{
		float4 position : SV_Position;
		float3 color : COLOR0;
	};

	[[vk::push_constant]]
	ConstantBuffer<float4x4> mvp;

	[shader("vertex")]
	VertexOutput vertexMain(uint id : SV_VulkanVertexID)
	{
		float3 positions[3] =
		{
			float3( 0.0, -0.5, 0.0),
			float3(-0.5,  0.5, 0.0),
			float3( 0.5,  0.5, 0.0)
		};

		float3 colors[3] =
		{
			float3(1.0, 0.0, 0.0),
			float3(0.0, 1.0, 0.0),
			float3(0.0, 0.0, 1.0)
		};

		VertexOutput output;
		output.position = mul(mvp, float4(positions[id].xyz, 1.0));
		output.color = colors[id];
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

int main()
{
	SlateApp app({
		.Title = "SlateVK - Window",
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

	if (!context->CreateGraphicsPipeline(vertexShader, fragmentShader))
		return -1;

	while (app.IsRunning())
	{
		app.ProcessEvents();

		if (!app.IsRunning())
			break;

		int width = 0;
		int height = 0;

		SDL_GetWindowSizeInPixels(app.GetNativeWindow(), &width, &height);

		glm::mat4 model(1.0f);
		glm::mat4 view = glm::lookAtRH(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		glm::mat4 projection = glm::perspectiveRH_ZO(glm::radians(60.0f), static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f);

		glm::mat4 mvp = projection * view * model;

		// projection[1][1] *= -1.0f;

		context->Render(glm::value_ptr(mvp));
	}

	return 0;
}