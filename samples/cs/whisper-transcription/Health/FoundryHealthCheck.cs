using Microsoft.Extensions.Diagnostics.HealthChecks;

namespace WhisperTranscription;

public class FoundryHealthCheck : IHealthCheck
{
    private readonly FoundryModelService _modelService;

    public FoundryHealthCheck(FoundryModelService modelService)
    {
        _modelService = modelService;
    }

    public async Task<HealthCheckResult> CheckHealthAsync(
        HealthCheckContext context,
        CancellationToken cancellationToken = default)
    {
        try
        {
            var model = await _modelService.GetModelAsync();
            return HealthCheckResult.Healthy($"Model available: {model.Id}");
        }
        catch (Exception ex)
        {
            return HealthCheckResult.Unhealthy("Foundry Local unavailable", ex);
        }
    }
}
