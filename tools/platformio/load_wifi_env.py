Import("env")

import os


PROJECT_DIR = env["PROJECT_DIR"]
DOTENV_PATH = os.path.join(PROJECT_DIR, ".env")


REQUIRED_STRING_VARS = {
    "DASH_WIFI_SSID": "WIFI_SSID",
    "DASH_WIFI_PASSWORD": "WIFI_PASSWORD",
}

OPTIONAL_VARS = {
    "DASH_LOCATION_LAT": ("LOCATION_LAT", float),
    "DASH_LOCATION_LON": ("LOCATION_LON", float),
    "DASH_TIMEZONE_POSIX": ("TIMEZONE_POSIX", str),
}


def cpp_string_literal(value):
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'\\"{escaped}\\"'


def load_dotenv(path):
    values = {}

    if not os.path.exists(path):
        return values

    with open(path, encoding="utf-8") as dotenv_file:
        for raw_line in dotenv_file:
            line = raw_line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue

            key, value = line.split("=", 1)
            key = key.strip()
            value = value.strip()

            if not key:
                continue

            if len(value) >= 2 and value[0] == value[-1] and value[0] in ('"', "'"):
                value = value[1:-1]

            values[key] = value

    return values


dotenv_values = load_dotenv(DOTENV_PATH)


def resolve_env_value(name):
    return os.getenv(name) or dotenv_values.get(name)


missing_vars = [name for name in REQUIRED_STRING_VARS if not resolve_env_value(name)]
if missing_vars:
    missing_list = ", ".join(missing_vars)
    raise RuntimeError(
        f"Missing required Wi-Fi environment variables: {missing_list}. "
        "Set them in the shell or in .env before building dash_35."
    )

cpp_defines = []

for env_name, define_name in REQUIRED_STRING_VARS.items():
    cpp_defines.append((define_name, cpp_string_literal(resolve_env_value(env_name))))

for env_name, (define_name, converter) in OPTIONAL_VARS.items():
    raw_value = resolve_env_value(env_name)
    if not raw_value:
        continue

    if converter is float:
        converter(raw_value)
        cpp_defines.append((define_name, raw_value))
    else:
        cpp_defines.append((define_name, cpp_string_literal(raw_value)))

env.Append(CPPDEFINES=cpp_defines)
