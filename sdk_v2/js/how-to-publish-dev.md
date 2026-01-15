#  How to Publish a Dev Build

This guide outlines the steps to publish a scoped development build of the Foundry Local SDK to npm.

## Prerequisites
- An **npm account** (created at npmjs.com)
- An **Access Token** (generated in your npm account settings)

## Instructions

### 1. Setup Authentication
First, configure your local npm registry with your authentication token. Replace `{NPM_AUTH_TOKEN}` with your actual token.

```bash
npm config set //registry.npmjs.org/:_authToken={NPM_AUTH_TOKEN}
```

### 2. Initialize Scoped Package
Initialize a new scoped package properly using your npm username. Replace `{USERNAME}` with your npm username.

```bash
npm init --scope=@{USERNAME}
```
> **Note:** Follow the interactive prompts to generate a custom `package.json`.

### 3. Build the Project
Compile the TypeScript source code.

```bash
npm run build
```

### 4. Pack the Artifacts
Create the distribution tarball. This will generate a `.tgz` file containing the `dist/` and `script/` directories, along with the `README.md` and `package.json`.

```bash
npm pack
```

### 5. Publish to npm
Publish the generated tarball to the public registry. Replace `{TGZ_FILEPATH}` with the path to the file generated in the previous step.

```bash
npm publish {TGZ_FILEPATH} --access public
```

---

 **Reference:** For more details, see the [npm documentation on creating and publishing scoped public packages](https://docs.npmjs.com/creating-and-publishing-scoped-public-packages).
