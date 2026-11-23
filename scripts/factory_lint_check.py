"""Evaluate saved lint results for the factory proof gate."""


def check(event, _configuration):
    result = event["input"]
    if result["exit_status"] == 0:
        return []
    return [f"Lint failed: {result['command']} (exit {result['exit_status']})"]
