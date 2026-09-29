<div align="center">

# 🛡️ PRAHARI
### Portable Rover for Autonomous Hazard Assessment & Rescue Intelligence

**An autonomous mine-rescue rover that goes into danger first — so a human doesn't have to.**

[![Platform](https://img.shields.io/badge/platform-Arduino%20Mega%20%7C%20ESP32-blue)]()
[![Language](https://img.shields.io/badge/firmware-C%2B%2B%20(Arduino)-orange)]()
[![Dashboard](https://img.shields.io/badge/dashboard-HTML%2FJS%2FMQTT-green)]()
[![ML](https://img.shields.io/badge/ML-Python%20%7C%20scikit--learn-yellow)]()
[![Event](https://img.shields.io/badge/Smart%20India%20Hackathon-2026-navy)]()

*Smart India Hackathon 2026 · Problem Statement SIH26039 · Team Cipher*

</div>

---

## 📖 Overview

Underground coal mines kill: **226 workers died in India's coal and lignite mines
between 2020–2024**. After a collapse, rescue teams currently enter an unknown
atmosphere **blind** — no data on toxic gas, temperature, or trapped workers.

**PRAHARI** is a low-cost (~₹36,000) autonomous rover that enters first. It:

- 🧭 **Navigates autonomously** — avoids obstacles using ultrasonic + LiDAR ToF
- 🌫️ **Senses the atmosphere** — CH₄, CO, temperature, humidity — and scores it
  against **DGMS / CMR 2017 statutory limits** to return a simple **GO / NO-GO** call
- 🛑 **Fails safe** — an independent IMU tilt cut-off stops the motors if the rover
  overturns, even if communications drop
- 🔥 **Detects hazards** — flame/fire and acoustic (tap) signatures of trapped workers
- ☁️ **Streams live** to a **cloud control-station dashboard** over MQTT
- 🤖 **Predicts danger early** — an ML model forecasts gas rate-of-rise, giving an
  **average ~30-second early warning before the DGMS limit is crossed**

---

## 🎥 Demo

> - **Live dashboard:** `https://widhia-solanki.github.io/PRAHARI/dashboard/`
> - **Demo video:** `<your video link>`

*(Tip: the dashboard has a built-in **Demo Mode** button that replays a full
rescue mission with no hardware — great for the portfolio viewer.)*

---

## 🏗️ System Architecture

