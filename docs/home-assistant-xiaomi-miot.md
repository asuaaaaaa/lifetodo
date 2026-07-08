# Home Assistant 与 Xiaomi Miot 接入

目标：让 Home Assistant 读取 LifeTodo 今日状态，并通过 `al-one/hass-xiaomi-miot` 控制已有米家设备或小爱音箱联动。

## 架构

```text
LifeTodo API
  -> Home Assistant REST sensors / rest_command
  -> Xiaomi Miot integration
  -> 米家设备 / 小爱音箱
```

LifeTodo 不伪装成米家官方设备。米家设备仍由 Xiaomi Miot 集成接入 Home Assistant，LifeTodo 只提供稳定的 HTTP 状态和动作接口。

参考集成：

```text
https://github.com/al-one/hass-xiaomi-miot
```

## LifeTodo API

状态接口：

```text
GET http://120.55.46.251/api/ha/status?home=demo-home
```

返回示例：

```json
{
  "homeId": "demo-home",
  "date": "2026-07-08",
  "summary": {
    "todayTotal": 3,
    "completedToday": 1,
    "remainingToday": 2,
    "hasStrongAlert": false,
    "strongAlertCount": 0
  },
  "weather": {
    "location": "北京朝阳 时间国际",
    "temperatureC": 29,
    "condition": "晴",
    "rainExpected": true,
    "rainText": "近期有雨",
    "precipitationProbability": 23,
    "updatedAt": "2026-07-08T03:06:02.371Z"
  },
  "devices": {
    "total": 1,
    "onlineCount": 1,
    "online": [
      {
        "id": "entry",
        "name": "门口屏",
        "location": "",
        "lastSeenAt": "2026-07-08T08:03:00.000Z"
      }
    ]
  },
  "tasks": [
    {
      "id": "take_umbrella",
      "title": "出门带伞",
      "label": "天气提醒",
      "assigneeId": "m1",
      "missedCount": 0,
      "overdueType": "",
      "isStrongAlert": false
    }
  ]
}
```

完成任务接口：

```text
POST http://120.55.46.251/api/ha/tasks/{taskId}/complete?home=demo-home
Content-Type: application/json

{"completed":true}
```

## Home Assistant REST 传感器

把下面内容合并到 Home Assistant 的 `configuration.yaml`，然后重启 Home Assistant。

```yaml
rest:
  - resource: "http://120.55.46.251/api/ha/status?home=demo-home"
    scan_interval: 60
    sensor:
      - name: "LifeTodo 今日剩余任务"
        unique_id: lifetodo_remaining_today
        value_template: "{{ value_json.summary.remainingToday }}"
        unit_of_measurement: "项"
        json_attributes_path: "$.summary"
        json_attributes:
          - todayTotal
          - completedToday
          - hasStrongAlert
          - strongAlertCount

      - name: "LifeTodo 当前温度"
        unique_id: lifetodo_temperature
        value_template: "{{ value_json.weather.temperatureC }}"
        unit_of_measurement: "°C"
        device_class: temperature
        state_class: measurement
        json_attributes_path: "$.weather"
        json_attributes:
          - location
          - condition
          - rainText
          - precipitationProbability
          - updatedAt

      - name: "LifeTodo 在线设备数"
        unique_id: lifetodo_online_devices
        value_template: "{{ value_json.devices.onlineCount }}"
        unit_of_measurement: "台"

binary_sensor:
  - platform: rest
    name: "LifeTodo 可能下雨"
    unique_id: lifetodo_rain_expected
    resource: "http://120.55.46.251/api/ha/status?home=demo-home"
    scan_interval: 60
    value_template: "{{ value_json.weather.rainExpected }}"

  - platform: rest
    name: "LifeTodo 强提醒"
    unique_id: lifetodo_strong_alert
    resource: "http://120.55.46.251/api/ha/status?home=demo-home"
    scan_interval: 60
    value_template: "{{ value_json.summary.hasStrongAlert }}"
```

## Home Assistant 完成任务动作

如果要从 HA 自动化里完成指定任务，先配置一个 `rest_command`：

```yaml
rest_command:
  lifetodo_complete_task:
    url: "http://120.55.46.251/api/ha/tasks/{{ task_id }}/complete?home=demo-home"
    method: post
    headers:
      Content-Type: "application/json"
    payload: '{"completed": true, "source": "home-assistant"}'
```

调用示例：

```yaml
service: rest_command.lifetodo_complete_task
data:
  task_id: take_umbrella
```

## Xiaomi Miot 联动示例

安装并配置 Xiaomi Miot：

1. 在 HACS 里搜索并安装 `Xiaomi Miot`。
2. 在 Home Assistant 的“设备与服务”里添加 `Xiaomi Miot`。
3. 使用小米账号或 host/token 接入已有米家设备。
4. 确认小爱音箱、灯、插座或其他提醒设备已经出现在 Home Assistant 实体列表里。

### 下雨提醒

下面示例在 LifeTodo 判断可能下雨时，打开一个米家灯光实体。把 `light.xiaomi_lamp` 换成你自己的实体 ID。

```yaml
automation:
  - alias: "LifeTodo 下雨时打开提醒灯"
    mode: single
    trigger:
      - platform: state
        entity_id: binary_sensor.lifetodo_可能下雨
        to: "on"
    action:
      - service: light.turn_on
        target:
          entity_id: light.xiaomi_lamp
        data:
          brightness_pct: 80
```

### 强提醒任务

下面示例在出现强提醒任务时调用小爱音箱播报。不同小爱实体的服务名可能不同，按 Xiaomi Miot 暴露的实体服务调整。

```yaml
automation:
  - alias: "LifeTodo 强提醒触发小爱播报"
    mode: single
    trigger:
      - platform: state
        entity_id: binary_sensor.lifetodo_强提醒
        to: "on"
    action:
      - service: notify.mobile_app_phone
        data:
          title: "LifeTodo 强提醒"
          message: "有家庭任务连续未完成，请打开 LifeTodo 查看。"
```

如果你的小爱音箱实体支持 TTS 或 Miot action，可以把 `notify.mobile_app_phone` 替换成对应服务。

## 调试

用浏览器或终端先确认 API 可访问：

```bash
curl "http://120.55.46.251/api/ha/status?home=demo-home"
```

如果 Home Assistant 里传感器不可用：

- 确认 Home Assistant 能访问 `120.55.46.251`。
- 在“开发者工具 -> 状态”里搜索 `lifetodo`。
- 查看 Home Assistant 日志里的 REST 解析错误。
- 如果 Xiaomi Miot 实体不可用，先在 Xiaomi Miot 集成页面确认小米账号、地区和设备在线状态。
