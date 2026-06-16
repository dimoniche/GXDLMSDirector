# GXDLMSDirector (C++ / Qt6)

Кроссплатформенный порт [GXDLMSDirector](https://github.com/Gurux/GXDLMSDirector) — приложения для работы с DLMS/COSEM-счётчиками (электричество, газ, вода).

Оригинал написан на C# / WinForms. Эта версия использует **Qt6** для UI и **[Gurux.DLMS.cpp](https://github.com/Gurux/Gurux.DLMS.cpp)** для протокола DLMS/COSEM.

## Требования

- CMake 3.16+
- C++17 компилятор
- Qt6 (Core, Widgets, SerialPort, Network, Concurrent, Xml, Test — для unit-тестов)
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

### Unit-тесты

```bash
cmake .. -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt@6 -DBUILD_TESTS=ON
cmake --build . -j8
ctest --test-dir unit_tests --output-on-failure
```

Покрытие: `ProjectSerializer`, `VariantConverter`, `AssociationViewParser`, `MacroSerializer`, `TraceFormatter`.

### Packaging

Скрипты собирают release и кладут артефакты в `dist/`:

```bash
# macOS .dmg (macdeployqt + ad-hoc codesign)
./packaging/build-dmg.sh
# или
./packaging/package.sh

# Linux AppImage (linuxdeploy + linuxdeploy-plugin-qt)
./packaging/build-appimage.sh
```

Переменные окружения: `BUILD_DIR`, `DIST_DIR`, `CMAKE_PREFIX_PATH` (macOS, путь к Qt6).

Версия задаётся в файле [`VERSION`](VERSION) (единый источник для CMake, packaging и приложения).

### Установка

```bash
# Сборка пакета
./packaging/package.sh

# Установка из build/dist (macOS -> /Applications, Linux -> ~/.local/bin)
./install.sh
```

| Платформа | Артефакт |
|-----------|----------|
| macOS | `dist/GXDLMSDirector-<version>-macOS.dmg` |
| Linux | `dist/GXDLMSDirector-<version>-x86_64.AppImage` |

Запуск:

```bash
open GXDLMSDirector.app          # macOS
./GXDLMSDirector                 # Linux
```

## Возможности

### Подключение и DLMS

- **Serial** и **TCP**
- LN/SN referencing, HDLC/Wrapper/PLC, адреса, аутентификация, **DLMS security** (GAK/GUEK)
- Подключение / отключение / association view
- Trace TX/RX (hex / XML / PDU), notifications
- **Force Read**, **Cancel** длительных операций

### Объекты и чтение

- Дерево COSEM-объектов с **группировкой по типу** (View → Groups)
- Вкладки **Tree** / **Object List**
- **Read** — объект, группа типа или всё устройство (Ctrl+R, контекстное меню)
- Таблица **Attributes** с тип-aware редактированием и записью
- Вкладка **Methods**, invoke + macro recording
- Редакторы: Clock, HDLC setup, Disconnect control, Octet string

### Profile Generic

- Вкладка **Buffer** при выборе Profile Generic (Attributes / Methods / **Buffer**)
- **By entry** по умолчанию (start index 1, row count 10), опционально **By range**
- Кнопка **Read** и таблица результатов в главном окне
- **Connection → Read Profile Generic...** / **Ctrl+Shift+G** — переключение на Buffer и чтение
- Массовое Read пропускает attr 2 (buffer) — используйте вкладку Buffer

### Проекты и данные

- **`.gxc`** (формат Qt-порта) + `.objects.xml`, multi-device, MRU, Clone
- **Save/Load values**, Find (Ctrl+F / F3)
- Manufacturer profiles (`~/Documents/GXDLMSDirector/Manufacturers.xml`)

### Инструменты

- **DLMS Translator**, **HDLC Address Scanner**, **PLC Discover**
- **Macro Editor** (`.gxm`), **Conformance** (MVP + external tests)

## Структура проекта

```
src/
  core/           # DLMS-слой (Communicator, Device, Serializer, …)
  ui/             # Qt6 UI
unit_tests/       # Qt Test (ProjectSerializer, VariantConverter)
packaging/        # build-dmg.sh, build-appimage.sh
third_party/
  gurux_dlms/     # Gurux.DLMS.cpp
cmake/
```

## Дорожная карта

| Фаза | Статус | Содержание |
|------|--------|------------|
| **1–7** | готово | MVP: connect, read/write, projects, macros, multi-device, groups, … |
| **8a** | готово | **Profile Generic в главном окне** — вкладка Buffer, entry по умолчанию, таблица результатов |
| **8b** | готово | **Unit-тесты** — serializer, variant, association view, macro, trace |
| **8c** | готово | **Packaging** — `.dmg` (macdeployqt), AppImage (linuxdeploy), `dist/` |
| **8d** | готово | **Security в Device Properties** — security mode, GAK/GUEK (hex), сохранение в `.gxc` |

**Не планируется:** Data Concentrator plugins; импорт `.gxc` из C# Director (свой XML-формат Qt-порта).

### Фаза 8a — Profile Generic в главном окне

Реализовано: вкладка **Buffer**, режим **By entry** по умолчанию, таблица в главном окне, меню/горячая клавиша запускают чтение.

### Фаза 8d — Security

Реализовано: вкладка **Security** в Device Properties — режим (None / Auth / Encrypt / Auth+Encrypt), ключи в hex, сохранение в проект.

### Фаза 8c — Packaging

- **macOS:** Release-сборка, `macdeployqt` (Qt в `.app`), ad-hoc codesign, `.dmg` в `dist/`
- **Linux:** `cmake --install` + `linuxdeploy` + plugin Qt → AppImage в `dist/`
- Обёртка: `./packaging/package.sh` (выбор по ОС)

## Лицензия

GPL-2.0 (как оригинальный GXDLMSDirector и Gurux.DLMS.cpp)

## Ссылки

- [Gurux DLMS Director (оригинал)](https://github.com/Gurux/GXDLMSDirector)
- [Gurux.DLMS.cpp](https://github.com/Gurux/Gurux.DLMS.cpp)
- [Документация Gurux DLMS](https://www.gurux.fi/Gurux.DLMS)
