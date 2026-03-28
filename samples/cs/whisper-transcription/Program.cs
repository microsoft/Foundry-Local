using Microsoft.AspNetCore.Mvc;
using WhisperTranscription;

var builder = WebApplication.CreateBuilder(args);

var configuredUrls = builder.Configuration["ASPNETCORE_URLS"] ?? builder.Configuration["urls"];
if (string.IsNullOrWhiteSpace(configuredUrls))
{
    // Keep the sample local-only by default because the upload API is intended for same-machine use.
    builder.WebHost.UseUrls("http://127.0.0.1:5000", "http://localhost:5000");
}

builder.Services.Configure<FoundryOptions>(
    builder.Configuration.GetSection(FoundryOptions.SectionName));
builder.Services.AddSingleton<FoundryModelService>();
builder.Services.AddSingleton<TranscriptionService>();
builder.Services.AddHealthChecks()
    .AddCheck<FoundryHealthCheck>("foundry");
builder.Services.AddEndpointsApiExplorer();
builder.Services.AddProblemDetails();
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

app.MapGet("/api/health/status", async (
    [FromServices] FoundryModelService modelService,
    [FromServices] IWebHostEnvironment environment) =>
{
    try
    {
        var model = await modelService.GetModelAsync();
        var isCached = await model.IsCachedAsync();
        return Results.Ok(new HealthStatusResponse(
            Status: "Healthy",
            Model: model.Id,
            Cached: isCached));
    }
    catch (Exception ex)
    {
        return Results.Json(
            new HealthStatusResponse(
                Status: "Degraded",
                Error: environment.IsDevelopment() ? ex.Message : "Foundry Local is unavailable."),
            statusCode: 503);
    }
}).WithName("GetHealthStatus")
  .Produces<HealthStatusResponse>(200, "application/json")
  .Produces<HealthStatusResponse>(503, "application/json");

app.MapPost("/v1/audio/transcriptions", async (
    [FromServices] TranscriptionService svc,
    [FromForm] IFormFile file,
    [FromForm] string? model,
    [FromForm] string? format,
    CancellationToken ct) =>
{
    if (file is null || file.Length == 0)
    {
        return Results.Problem(
            statusCode: 400,
            title: "Invalid transcription request",
            detail: "No audio file provided.");
    }

    // Save upload to temp file
    var tmp = Path.Combine(Path.GetTempPath(), Guid.NewGuid() + Path.GetExtension(file.FileName));
    await using (var fs = File.Create(tmp))
    {
        await file.CopyToAsync(fs, ct);
    }

    try
    {
        var result = await svc.TranscribeAsync(tmp, model, ct);
        var outputFormat = format?.ToLowerInvariant() ?? "text";
        return outputFormat switch
        {
            "json" => Results.Ok(new TranscriptionResponse(result.Text, result.ModelId)),
            _ => Results.Text(result.Text, "text/plain"),
        };
    }
    finally
    {
        try { File.Delete(tmp); } catch { /* cleanup best-effort */ }
    }
}).WithName("TranscribeAudio")
  .DisableAntiforgery()
        .Produces<TranscriptionResponse>(200, "application/json")
        .Produces<string>(200, "text/plain")
        .ProducesProblem(400)
        .ProducesProblem(500);

app.MapFallbackToFile("index.html");

app.Run();

sealed record HealthStatusResponse(string Status, string? Model = null, bool? Cached = null, string? Error = null);
sealed record TranscriptionResponse(string Text, string Model);
