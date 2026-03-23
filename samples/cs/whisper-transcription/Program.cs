using Microsoft.AspNetCore.Mvc;
using WhisperTranscription;

var builder = WebApplication.CreateBuilder(args);

builder.Services.Configure<FoundryOptions>(
    builder.Configuration.GetSection(FoundryOptions.SectionName));
builder.Services.AddSingleton<FoundryModelService>();
builder.Services.AddSingleton<TranscriptionService>();
builder.Services.AddHealthChecks()
    .AddCheck<FoundryHealthCheck>("foundry");
builder.Services.AddEndpointsApiExplorer();
builder.Services.AddSwaggerGen();

builder.Services.ConfigureHttpJsonOptions(options =>
{
    options.SerializerOptions.WriteIndented = true;
    options.SerializerOptions.PropertyNamingPolicy = System.Text.Json.JsonNamingPolicy.CamelCase;
});

var app = builder.Build();

app.UseMiddleware<ErrorHandlingMiddleware>();
app.UseDefaultFiles();
app.UseStaticFiles();

if (app.Environment.IsDevelopment())
{
    app.UseSwagger();
    app.UseSwaggerUI();
}

app.MapHealthChecks("/health");

app.MapGet("/api/health/status", async ([FromServices] FoundryModelService modelService) =>
{
    try
    {
        var model = await modelService.GetModelAsync();
        var isCached = await model.IsCachedAsync();
        return Results.Ok(new
        {
            status = "Healthy",
            model = model.Id,
            cached = isCached,
        });
    }
    catch (Exception ex)
    {
        return Results.Ok(new
        {
            status = "Degraded",
            error = ex.Message,
        });
    }
}).WithName("GetHealthStatus");

app.MapPost("/v1/audio/transcriptions", async (
    [FromServices] TranscriptionService svc,
    [FromForm] IFormFile file,
    [FromForm] string? model,
    [FromForm] string? format) =>
{
    if (file is null || file.Length == 0)
    {
        return Results.BadRequest(new { error = "No audio file provided" });
    }

    // Save upload to temp file
    var tmp = Path.Combine(Path.GetTempPath(), Guid.NewGuid() + Path.GetExtension(file.FileName));
    await using (var fs = File.Create(tmp))
    {
        await file.CopyToAsync(fs);
    }

    try
    {
        var result = await svc.TranscribeAsync(tmp, model);
        var outputFormat = format?.ToLowerInvariant() ?? "text";
        return outputFormat switch
        {
            "json" => Results.Ok(new { text = result.Text, model = result.ModelId }),
            _ => Results.Text(result.Text, "text/plain"),
        };
    }
    finally
    {
        try { File.Delete(tmp); } catch { /* cleanup best-effort */ }
    }
}).WithName("TranscribeAudio")
  .DisableAntiforgery()
  .Produces(200)
  .ProducesProblem(400)
  .ProducesProblem(500);

app.MapFallbackToFile("index.html");

app.Run();
