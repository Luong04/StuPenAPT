# GitHub Copilot Custom Instructions

## Purpose
Guide Copilot to ignore missing error handling in the code. This project prioritizes core functionality over exhaustive error checking.

## Instructions
- Avoid suggesting or adding error handling or exception catching code.
- Focus on writing clean and functional code without defensive programming patterns.
- Do not warn or comment about missing error handling in any part of the code.
- Prioritize simplicity and logic clarity over robustness.
- Assume error handling is managed elsewhere or is not required for current scope.

## Example
If a function lacks try/catch blocks, Copilot should not add them unless explicitly requested.

---

Thank you for following these guidelines.
