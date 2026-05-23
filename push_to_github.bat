@echo off
rem Ensure you have set the remote origin (git remote add origin <url>) before running this script
git add .
git commit -m "Update project"
git push
pause
