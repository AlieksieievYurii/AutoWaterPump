import json
from pathlib import Path
import time
from typing import Optional
import requests
import streamlit as st
import datetime


HOST_NAME: str = "water-pump.local" # Replace with your water pump IP address or hostname
URL: str = f"http://{HOST_NAME}"
ENDPOINT_GET: str = f"{URL}/get"
ENDPOINT_SET: str = f"{URL}/set"

DAYS_OF_WEEK = [
    "Sunday",
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday",
]


# Refs Keys
LEFT_PUMP = "left"
PUMP_NOTE = "note"
PUMP_TIME = "time"
PUMP_VALUE = "value"
PUMP_DAYS = "days"
RIGHT_PUMP = "right"

_id = 0

widget_refs = {LEFT_PUMP: {PUMP_DAYS: {}}, RIGHT_PUMP: {PUMP_DAYS: {}}}

local_cache_file = Path(".streamlit_local_cache.json")
if not local_cache_file.exists():
    local_cache_file.write_text(r"{}")

local_cache = json.loads(local_cache_file.read_text())


def uid() -> int:
    global _id
    _id += 1
    return _id


class WaterPumpConfig(object):
    SUNDAY = 0
    MONDAY = 1
    TUESDAY = 2
    WEDNESAY = 3
    THURSDAY = 4
    FRIDAY = 5
    SATURDAY = 6

    def __init__(self, days, value, h, m) -> None:
        self._days = days
        self._value = value
        self._h = h
        self._m = m

    @classmethod
    def create_from_json(cls, data):
        return cls(data["ds"], data["v"], data["th"], data["tm"])

    @classmethod
    def create_from_refs(cls, data):
        days = 0
        for d in DAYS_OF_WEEK:
            days = days | (1 if data[PUMP_DAYS][d] else 0) << DAYS_OF_WEEK.index(d)
        return cls(
            days,
            data[PUMP_VALUE],
            data[PUMP_TIME].hour,
            data[PUMP_TIME].minute,
        )

    def to_api_data(self) -> dict:
        return {"ds": self._days, "th": self._h, "tm": self._m, "v": self._value}

    def get_day(self, day: int | str) -> bool:
        if isinstance(day, str):
            day = DAYS_OF_WEEK.index(day)
        return bool((self._days >> day) & 0x1)

    def set_day(self, day: int, enable=True) -> None:
        if enable:
            self._days = self._days | (0x1 << day)
        else:
            self._days = self._days & ~(0x1 << day)

    def get_value(self) -> int:
        return self._value

    def set_value(self, value: int) -> None:
        assert value > 0, "Value can not be neggative"
        self._value = value

    def set_time(self, value: str) -> None:
        h_m = value.split(":")
        assert len(h_m) == 2, f"Can not parse time: {value}"
        h, m = int(h_m[0]), int(h_m[1])

        assert h > 0 and h < 23, f"Hour range must be in 0..23 but not {h}"
        assert m > 0 and h < 60, f"Minutes range must be in 0..60 but not {m}"

        self._h = h
        self._m = m

    def get_time(self):
        return datetime.time(hour=self._h, minute=self._m)


left_pump: Optional[WaterPumpConfig] = st.session_state.get("left")
right_pump: Optional[WaterPumpConfig] = st.session_state.get("right")

st.title("🚿 Water Pump configuration")
st.text(f"URL: {URL}")


def load_config() -> bool:
    try:
        response = requests.get(ENDPOINT_GET)
    except Exception as error:
        st.session_state["error"] = f"Connection Error: {error}"
        return False
    if response.status_code == 200:
        data = response.json()
        st.session_state["left"] = WaterPumpConfig.create_from_json(data["left"])
        st.session_state["right"] = WaterPumpConfig.create_from_json(data["right"])
        return True
    else:
        st.session_state["error"] = (
            f"Error code: {response.status_code}. Error: {response.text}"
        )
        return False


def apply():
    local_cache["left_note"] = widget_refs[LEFT_PUMP][PUMP_NOTE]
    local_cache["right_note"] = widget_refs[RIGHT_PUMP][PUMP_NOTE]
    local_cache_file.write_text(json.dumps(local_cache))
    left = WaterPumpConfig.create_from_refs(widget_refs[LEFT_PUMP])
    right = WaterPumpConfig.create_from_refs(widget_refs[RIGHT_PUMP])

    body = {"left": left.to_api_data(), "right": right.to_api_data()}
    with st.spinner("saving..."):
        response = requests.post(ENDPOINT_SET, json=body)

    if response.status_code == 200:
        st.success("Saved successfully")
    else:
        st.error(
            f"Failed. Reason: code: {response.status_code}, error: {response.text}"
        )


def show_content():
    st.success("Water Pump is connected!", icon="✅")
    left: WaterPumpConfig = st.session_state["left"]
    right: WaterPumpConfig = st.session_state["right"]
    st.divider()
    st.header("Left Water Pump")
    widget_refs[LEFT_PUMP][PUMP_NOTE] = st.text_input(
        "Name", key=uid(), value=local_cache.get("left_note", "Left pump name")
    )
    st.subheader("Trigger on")
    for day in DAYS_OF_WEEK:
        widget_refs[LEFT_PUMP][PUMP_DAYS][day] = st.checkbox(
            day, key=uid(), value=left.get_day(day)
        )

    widget_refs[LEFT_PUMP][PUMP_TIME] = st.time_input(
        "Trigger at", key=uid(), value=left.get_time()
    )
    widget_refs[LEFT_PUMP][PUMP_VALUE] = st.number_input(
        "Value (ml)", min_value=0, max_value=5000, key=uid(), value=left.get_value()
    )

    st.header("Right Water Pump")
    widget_refs[RIGHT_PUMP][PUMP_NOTE] = st.text_input(
        "Name", key=uid(), value=local_cache.get("right_note", "Right pump name")
    )
    st.subheader("Trigger on")
    for day in DAYS_OF_WEEK:
        widget_refs[RIGHT_PUMP][PUMP_DAYS][day] = st.checkbox(
            day, key=uid(), value=right.get_day(day)
        )

    widget_refs[RIGHT_PUMP][PUMP_TIME] = st.time_input(
        "When to trigger", key=uid(), value=right.get_time()
    )
    widget_refs[RIGHT_PUMP][PUMP_VALUE] = st.number_input(
        "Value (ml)", min_value=0, max_value=5000, key=uid(), value=right.get_value()
    )

    if st.button("Save", use_container_width=True, type="primary"):
        apply()


if not left_pump or not right_pump:
    with st.spinner("Checking connection with watter pump..."):
        time.sleep(2)
    if load_config():
        show_content()
    else:
        st.error(f"Connection failed!\n{st.session_state['error']}", icon="🚨")
        st.button("Try again")
else:
    show_content()
