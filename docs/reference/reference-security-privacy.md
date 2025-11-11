# Best practices and troubleshooting guide for Foundry Local

This document provides best practices and troubleshooting tips for Foundry Local.

## Security and privacy considerations

Foundry Local is designed with privacy and security as core principles:

- **Local processing**: All data processed by Foundry Local remains on your device and is never sent to Microsoft or any external services.
- **Privacy**: see [Microsoft Privacy Statement](https://aka.ms/privacy).
- **Air-gapped environments**: Foundry Local can be used in disconnected environments after initial model download.

## Security best practices

- Use Foundry Local in environments that comply with your organization's security policies.
- When handling sensitive data, ensure your device meets your organization's security requirements.
- Use disk encryption on devices where cached models might contain sensitive fine-tuning data.

## Licensing considerations

When using Foundry Local, be aware of the licensing implications for the models you run. You can view full terms of model license for each model in the model catalog using:

```bash
foundry model info <model> --license
```

Models available through Foundry Local are subject to their original licenses:

- Open-source models maintain their original licenses (e.g., Apache 2.0, MIT).
- Commercial models may have specific usage restrictions or require separate licensing.
- Always review the licensing information for each model before deploying in production.

## Production deployment scope

Foundry Local is designed for on-device inference and _not_ distributed, containerized, or multi-machine production deployments.

## Troubleshooting

### Common issues and solutions

| Issue                      | Possible Cause                            | Solution                                                                            |
| -------------------------- | ----------------------------------------- | ----------------------------------------------------------------------------------- |
| Slow inference             | CPU-only model with large parameter count | Use GPU-optimized model variants when available                                     |
| Model download failures    | Network connectivity issues               | Check your internet connection and run `foundry cache list` to verify cache status  |
| The service fails to start | Port conflicts or permission issues       | Try `foundry service restart` or report an issue with logs using `foundry zip-logs` |

### Improving performance

If you experience slow inference, consider the following strategies:

- Use GPU acceleration when available
- Identify bottlenecks by monitoring memory usage during inference
- Try more quantized model variants (like INT8 instead of FP16)
- Adjust batch sizes for non-interactive workloads
