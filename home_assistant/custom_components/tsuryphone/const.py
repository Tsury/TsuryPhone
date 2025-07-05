"""Constants for the TsuryPhone integration."""

DOMAIN = "tsuryphone"

# Platforms
PLATFORMS = [
    "binary_sensor",
    "button", 
    "number",
    "select",
    "sensor",
    "switch",
    "time",
]

# Configuration
CONF_HOST = "host"
CONF_PORT = "port"

# Default values
DEFAULT_PORT = 80
DEFAULT_SCAN_INTERVAL = 30

# Device info
MANUFACTURER = "TsuryPhone Project"
MODEL = "TsuryPhone"

# API endpoints
ENDPOINT_STATUS = "/status"
ENDPOINT_STATS = "/stats"
ENDPOINT_DND = "/dnd"
ENDPOINT_PHONEBOOK = "/phonebook"
ENDPOINT_SCREENED = "/screened"
ENDPOINT_ACTION_CALL = "/action/call"
ENDPOINT_ACTION_HANGUP = "/action/hangup"
ENDPOINT_ACTION_RESET = "/action/reset"
ENDPOINT_ACTION_REBOOT = "/action/reboot"
ENDPOINT_ACTION_RING = "/action/ring"

# Phone states
PHONE_STATES = [
    "Startup",
    "CheckHardware", 
    "CheckLine",
    "Idle",
    "InvalidNumber",
    "IncomingCall",
    "IncomingCallRing",
    "InCall",
    "Dialing",
]
