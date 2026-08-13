# Deployment Setup - Required Secrets

## GitHub Personal Access Token (GH_PAT)

The build workflow requires a GitHub Personal Access Token to clone RE-UE4SS submodules. Without this token, the workflow will fail silently at the submodule cloning step.

---

## Step 1: Create the PAT Token

1. Go to https://github.com/settings/tokens
2. Click "Generate new token (classic)" button
3. Give it a description: **"GitHub Actions Build Workflow - JakeSnowy Fork"**
4. **Select scopes:**
   - ✅ **Public repositories** (minimum required)
   - ❌ Read:Repository (not needed for public repos)
5. Click "Generate token"
6. **Copy the entire token string** (it will look like `ghp_xxxxxxxxxxxx`)

---

## Step 2: Add to Repository Settings

1. Go to your fork: https://github.com/Jakesnowy/sharing-base-resources
2. Click **Settings** tab (top navigation)
3. Scroll down and click **"Secrets and variables"** → **"Actions"**
4. Click **"New repository secret"** button
5. Enter details:
   - **Name:** `GH_PAT` (must be exactly this name, case-sensitive)
   - **Value:** Paste your token (long string starting with `ghp_...`)
6. Click **"Add secret"**

---

## Step 3: Verify Setup

After adding the secret:

1. Push a commit to trigger the workflow (e.g., edit any file and commit)
2. Go to **Actions** tab in your repo
3. Wait for the build to complete (~5-10 minutes)
4. Check that it downloads successfully without errors

---

## Troubleshooting

### Error: "submodule update failed"

**Cause:** PAT token missing or expired  
**Solution:** 
1. Verify PAT is valid by visiting https://github.com/ in browser (should not prompt for auth)
2. Regenerate PAT if needed
3. Re-add to repo secrets with exact name `GH_PAT`

### Error: "xmake configuration failed"

**Cause:** MSVC build tools not installed or misconfigured  
**Solution:**
1. Run locally to test: `.\build.ps1 -Bootstrap -UE4SSRoot D:\src\RE-UE4SS`
2. Follow prompts to install MSVC Build Tools
3. Retry workflow

### Error: "dllmain.cpp not found"

**Cause:** Source files weren't committed or git submodule not initialized  
**Solution:**
1. Verify `src/dllmain.cpp` exists in your repo
2. Run locally first to test setup
3. Commit changes and push again

---

## Alternative: Use Local Build Script

If you prefer not to use GitHub Actions, you can build locally using the provided script:

```powershell
# First-time setup or new machine (automatically installs dependencies):
.\build.ps1 -Bootstrap -UE4SSRoot D:\src\RE-UE4SS -Zip

# Subsequent builds (environment already ready):
.\build.ps1 -UE4SSRoot D:\src\RE-UE4SS -Zip
```

This produces the same output as GitHub Actions and is suitable for:
- Development/testing
- Distribution to friends
- CI/CD pipelines on your own server

---

## PAT Token Best Practices

✅ **DO:**
- Use minimal scopes (public repos only)
- Rotate token every 6-12 months
- Never commit token to repository
- Test locally before using in workflow

❌ **DON'T:**
- Use default "all repositories" scope
- Share token publicly
- Use token for anything other than this workflow
- Forget to rotate after security breach

---

## Workflow Triggers

The build runs automatically when:
- Push commits to `main` or `master` branch
- Files modified: `src/**`, `dist/**`, `.github/workflows/build.yml`
- Manual trigger via Actions tab ("Run workflow")

To change triggers, edit the `on:` section in `.github/workflows/build.yml`.

---

## Build Output Locations

After successful build, artifacts available at:
1. **GitHub Actions:** Artifacts tab (30-day retention)
2. **Local repository:** `dist/ModIntegratedStorageCpp/` folder and zip file
3. **Download URL:** `https://github.com/Jakesnowy/sharing-base-resources/releases` (if you create release)

---

## Release Creation (Optional)

To publish to GitHub Releases:

1. After workflow succeeds, go to **Releases** tab
2. Click **"Draft a new release"**
3. Fill details:
   - Tag version: `v4.1.2-fixes-<DATE>`
   - Title: "IntegratedStorageCpp v4.1.2-fixes"
   - Description: Link to README.md with instructions
4. Attach artifact from Actions tab or upload zip file
5. Click **"Publish release"**

---

**Last Updated:** 2024  
**Maintained by:** JakeSnowy (https://github.com/Jakesnowy/sharing-base-resources)  
**Base Project:** Sarfflow/IntegratedStorageCpp v4.1.2
