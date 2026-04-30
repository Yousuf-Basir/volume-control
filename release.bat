@echo off
set /p version="Enter version tag (e.g. v1.0.1): "

echo.
echo === Pushing changes to master ===
git add .
git commit -m "Release %version%"
git push origin master

echo.
echo === Creating and pushing tag %version% ===
git tag %version%
git push origin %version%

echo.
echo === Done! Check GitHub Actions for the build progress. ===
pause
