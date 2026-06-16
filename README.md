# GXDLMSDirector (C++ / Qt6)

Кроссплатформенный порт [GXDLMSDirector](https://github.com/Gurux/GXDLMSDirector) — приложения для работы с DLMS/COSEM-счётчиками (электричество, газ, вода).

Оригинал написан на C# / WinForms. Эта версия использует **Qt6** для UI и **[Gurux.DLMS.cpp](https://github.com/Gurux/Gurux.DLMS.cpp)** для протокола DLMS/COSEM.

## Требования

- CMake 3.16+
- C++17 компилятор
- Qt6 (Core, Widgets, SerialPort, Network, Concurrent, Xml)
- Git (для загрузки зависимости Gurux)

### macOS (Homebrew)

```bash
brew install cmake qt@6
```

## Сборка

```bash
git submodule update --init --recursive   # если third_party/gurux_dlms ещё не загружен
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt@6
cmake --build . -j$(sysctl -n hw.ncpu)
```

Запуск:

```bash
open GXDLMSDirector.app          # macOS
./GXDLMSDirector                 # Linux
```

## Возможности (MVP)

- Подключение по **Serial** и **TCP**
- Настройка DLMS-параметров (LN/SN, HDLC/Wrapper, адреса, аутентификация)
- Подключение / отключение / чтение association view
- Дерево COSEM-объектов и таблица атрибутов
- Trace-лог обмена (TX/RX hex)
- **Сохранение/загрузка проектов `.gxc`** (+ `.objects.xml` для COSEM-объектов)
- **Manufacturer settings** — профили производителей (`~/Documents/GXDLMSDirector/Manufacturers.xml`)
- **Запись атрибутов** — редактирование значений в таблице + Write Object
- **Profile Generic** — чтение по entry/range, таблица результатов
- **DLMS Translator** — hex PDU/data → XML (Tools menu)
- **HDLC Address Scanner** — перебор client/server адресов через SNRM
- **Macro Editor** — запись/воспроизведение `.gxm`, run/verify
- **Conformance** — negotiated conformance + external `.gxm` tests с HTML-отчётом
- **PLC Discover** — обнаружение PLC-счётчиков, MAC-адреса в свойствах устройства
- **Data Concentrators** — заглушка plugin-интерфейса (без vendor plugins)
- **COSEM object editors** — имена атрибутов/методов, тип-aware таблица, Clock editor
- **Add/Delete COSEM objects** — offline редактирование association view
- **Method invocation** — вкладка Methods, Invoke + macro recording
- **Octet string editor** — hex-редактор для бинарных атрибутов
- **HDLC setup editor** — выбор скорости + запись attr 2
- **Disconnect control editor** — статус + Remote disconnect/reconnect
- **Property table delegates** — bool и HDLC speed как combo
- **DLMS Notifications** — Start/Stop, вкладка Notifications, hex/XML/PDU
- **Trace modes** — View → Hex / XML / PDU / None, timestamps
- **Force Read** — чтение всех атрибутов объекта (включая static)
- **Cancel** — отмена длительных read-операций
- **Multi-device projects** — несколько счётчиков в одном `.gxc`
- **Recent projects** — MRU в меню File
- **Find / Find Next** — поиск COSEM-объектов в дереве (Ctrl+F / F3)
- **Groups** — группировка объектов по типу COSEM (View → Groups), вкладки Tree / Object List
- **Read selection** — чтение выбранного объекта, группы типа или устройства (Connection → Read, Ctrl+R, контекстное меню)
- **Clone device** — копия настроек и объектов
- **Save/Load values** — экспорт/импорт `.objects.xml`
- Асинхронные операции (не блокируют UI)

## Структура проекта

```
src/
  core/           # DLMS-слой (Communicator, Device, MediaConnection)
  ui/             # Qt6 UI (MainWindow, DevicePropertiesDialog)
third_party/
  gurux_dlms/     # Gurux.DLMS.cpp
cmake/            # CMake-модули
```

## Дорожная карта (порт с C#)

| Фаза | Функциональность |
|------|------------------|
| **1 (готово)** | Базовое подключение, чтение объектов, trace |
| **2 (готово)** | `.gxc` проекты, manufacturer settings, запись атрибутов |
| **3 (готово)** | Profile Generic, DLMS Translator, HDLC address scanner |
| **4 (готово)** | Макросы, conformance (MVP), PLC discover, data concentrator stub |
| **5 (готово)** | COSEM object editors: attributes/methods, add/delete, invoke |
| **6 (готово)** | HDLC/Disconnect editors, notifications, trace modes, force read, cancel |
| **7 (готово)** | Multi-device projects, MRU, find, clone, save/load values |

## Лицензия

GPL-2.0 (как оригинальный GXDLMSDirector и Gurux.DLMS.cpp)

## Ссылки

- [Gurux DLMS Director (оригинал)](https://github.com/Gurux/GXDLMSDirector)
- [Gurux.DLMS.cpp](https://github.com/Gurux/Gurux.DLMS.cpp)
- [Документация Gurux DLMS](https://www.gurux.fi/Gurux.DLMS)
