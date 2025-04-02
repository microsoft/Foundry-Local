# Foundry Local CLI Reference

This article provides a comprehensive reference for the AI Foundry Local command-line interface (CLI). The foundry CLI is structured into several categories to help you manage models, control the service, and maintain your local cache.

## Overview

To see all available commands, use the help option:

```bash
foundry --help
```

The foundry CLI is structured into these main categories:

- **Model**: Commands related to managing and running models
- **Service**: Commands for managing the AI Foundry Local service
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

The following table summarizes the commands related to managing and running the Foundry Local service:

| **Command**               | **Description**                                                    |
| ------------------------- | ------------------------------------------------------------------ |
| `foundry service --help`  | Displays all available service-related commands and their usage.   |
| `foundry service start`   | Starts the AI Foundry Local service.                               |
| `foundry service stop`    | Stops the AI Foundry Local service.                                |
| `foundry service restart` | Restarts the AI Foundry Local service.                             |
| `foundry service status`  | Displays the current status of the AI Foundry Local service.       |
| `foundry service ps`      | Lists all models currently loaded in the AI Foundry Local service. |
| `foundry service logs`    | Displays the logs of the AI Foundry Local service.                 |
| `foundry service set`     | Set configuration of the AI Foundry Local service.                 |

## Cache commands

The following table summarizes the commands related to managing the local cache where models are stored:

| **Command**                    | **Description**                                                |
| ------------------------------ | -------------------------------------------------------------- |
| `foundry cache --help`         | Displays all available cache-related commands and their usage. |
| `foundry cache pwd`            | Displays the current cache directory.                          |
| `foundry cache list`           | Lists all models stored in the local cache.                    |
| `foundry cache remove <model>` | Deletes a model from the local cache.                          |
| `foundry cache cd <path>`      | Changes the cache directory.                                   |
