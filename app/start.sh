#!/bin/bash

echo "Creating python env..."
cd /opt/stream_lit_application
python3 -m venv .env
source .env/bin/activate
pip install streamlit
exec streamlit run app.py