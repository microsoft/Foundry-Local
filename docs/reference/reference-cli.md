# Foundry Local CLI Reference

This article provides a comprehensive reference for the Foundry Local command-line interface (CLI). The foundry CLI is structured into several categories to help you manage models, control the service, and maintain your local cache.

## Overview

To see all available commands, use the help option:

```bash
foundry --help
```

The foundry CLI is structured into these main categories:

- **Model**: Commands related to managing and running models
- **Service**: Commands for managing the Foundry Local service
- **Cache**: Commands for managing the local cache where models are stored

## Model commands

The following table summarizes the commands related to managing and running models:

| **Command**                      | **Description**                                                                  |
| -------------------------------- | -------------------------------------------------------------------------------- |
| `foundry model --help`           | Displays all available model-related commands and their usage.                   |
| `foundry model run <model>`      | Runs a specified model, downloading it if not cached, and starts an interaction. |
| `foundry model list`             | Lists all available models for local use.                                        |
| `foundry model info <model>`     | Displays detailed information about a specific model.                            |
| `foundry model download <model>` | Downloads a model to the local cache without running it.                         |
| `foundry model load <model>`     | Loads a model into the service.                                                  |
| `foundry model unload <model>`   | Unloads a model from the service.                                                |

## Service commands

The following table summarizes the commands related to managing the Foundry Local service:

| **Command**               | **Description**                                                  |
| ------------------------- | ---------------------------------------------------------------- |
| `foundry service --help`  | Displays all available service-related commands and their usage. |
| `foundry service start`   | Starts the Foundry Local service.                                |
| `foundry service stop`    | Stops the Foundry Local service.                                 |
| `foundry service restart` | Restarts the Foundry Local service.                              |
| `foundry service status`  | Displays the current status of the Foundry Local service.        |
| `foundry service ps`      | Lists all models currently loaded in the Foundry Local service.  |
| `foundry service logs`    | Displays the logs of the Foundry Local service.                  |
| `foundry service set`     | Set configuration of the Foundry Local service.                  |

## Cache commands

The following table summarizes the commands related to managing the local cache where models are stored:

| **Command**                    | **Description**                                                |
| ------------------------------ | -------------------------------------------------------------- |
| `foundry cache --help`         | Displays all available cache-related commands and their usage. |
| `foundry cache pwd`            | Displays the current cache directory.                          |
| `foundry cache list`           | Lists all models stored in the local cache.                    |
| `foundry cache remove <model>` | Deletes a model from the local cache.                          |
| `foundry cache cd <path>`      | Changes the cache directory.                                   |

## Common CLI usage examples

### Quick start with a model

```bash
# Download and run a model interactively
foundry model run gpt-3.5

# Check model information before running
foundry model info gpt-3.5

# Download a model without running it
foundry model download llama-3-8b
```

### Managing the service

```bash
# Check service status
foundry service status

# View active models
foundry service ps

# Restart the service when troubleshooting
foundry service restart
```

### Working with the cache

```bash
# List cached models
foundry cache list

# Remove a model that's no longer needed
foundry cache remove old-model

# Change cache location to a larger drive
foundry cache cd /path/to/larger/drive
```

### Advanced usage

```bash
# View detailed model license information
foundry model info gpt-3.5 --license

# Generate diagnostic logs for support
foundry zip-logs

# Configure GPU settings for better performance
foundry service set --gpu 0
```
