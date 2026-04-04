// This file is used by Code Analysis to maintain SuppressMessage
// attributes that are applied to this project.
// Project-level suppressions either have no target or are given
// a specific target and scoped to a namespace, type, member, etc.

using System.Diagnostics.CodeAnalysis;

// Neutron code. Appears that the _releaser is deliberately not disposed of because it may be being used elsewhere
// due to being returned from the LockAsync method.
[assembly: SuppressMessage("IDisposableAnalyzers.Correctness", "IDISP002:Dispose member", Justification = "The _releaser is not disposed because it may be used elsewhere after being returned from the LockAsync method.", Scope = "member", Target = "~F:Microsoft.AI.Foundry.Local.Detail.AsyncLock._releaser")]
