#include "SlateApp.hpp"

#include <Slate/Slate.hpp>

constexpr const char* vertexShader = R"(
	struct VertexOutput
	{
		float4 position : SV_Position;
		float3 color : COLOR0;
	}

	[shader("vertex")]
	VertexOutput vertexMain(uint vertexID : SV_VulkanVertexID)
	{
		float3 positions[3] =
		{
			float3( 0.0, -0.5, 0.0),  // Top
			float3(-0.5,  0.5, 0.0), // Bottom right
			float3( 0.5,  0.5, 0.0) // Bottom left
		};

		float3 colors[3] =
		{
			float3(1.0, 0.0, 0.0),  // Red
			float3(0.0, 1.0, 0.0), // Blue
			float3(0.0, 0.0, 1.0) // Green
		};

		VertexOutput output;
		output.position = float4(positions[vertexID].x, positions[vertexID].y, positions[vertexID].z, 1.0);
		output.color = colors[vertexID];

		return output;
	}
)";

constexpr const char* fragmentShader = R"(
	struct FragmentInput
	{
		float4 position : SV_Position;
		float3 color : COLOR0;
	};

	[shader("fragment")]
	float4 fragmentMain(FragmentInput input) : SV_Target
	{
		return float4(input.color.r, input.color.g, input.color.b, 1.0);
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

		context->Render();
	}

	return 0;
}