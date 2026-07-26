/**
 * @file UI/uiMessages.hpp
 * @brief Тексты UI (UTF-8): MsgBox, статусы. На панель — через enc:: (KOI8-R).
 */

#pragma once

namespace uiMsg {

/* --- Заголовки MsgBox --- */
inline constexpr const char* kTitleFile = "Файл";
inline constexpr const char* kTitleGroup = "Группа";
inline constexpr const char* kTitleShow = "Шоу";
inline constexpr const char* kTitleBlock = "Блокировка";
inline constexpr const char* kTitleSettings = "Настройки";

/* --- Подтверждения (Yes/No) --- */
inline constexpr const char* kConfirmNewShow =
    "Вы уверены, что хотите создать новое шоу?";
inline constexpr const char* kConfirmNewShowDiscard =
    "Шоу не сохранено.\rСоздать новое?\rИзменения будут потеряны.";
inline constexpr const char* kConfirmOpenDiscard =
    "Шоу не сохранено.\rОткрыть другой файл?\rИзменения будут потеряны.";
inline constexpr const char* kConfirmOverwriteFile =
    "Файл с таким именем уже есть.\rЗаменить его?";
inline constexpr const char* kConfirmDeleteFile =
    "Вы уверены, что хотите удалить этот файл?";
inline constexpr const char* kConfirmOverwriteGroup =
    "В группе уже есть данные.\rЗаменить их новыми?";
inline constexpr const char* kConfirmClearGroup =
    "Вы уверены, что хотите очистить группу?";
inline constexpr const char* kConfirmExitShow =
    "Вы уверены, что хотите выйти из режима шоу?";

/* --- Информационные (OK) --- */
inline constexpr const char* kFileSaved = "Файл успешно сохранён.";
inline constexpr const char* kFileDeleted = "Файл удалён.";
inline constexpr const char* kOk = "Готово";
inline constexpr const char* kDateTimeClamped =
    "Значение даты или времени вне диапазона.\rУстановлено ближайшее допустимое.";

/* --- Статусы MBrowser --- */
inline constexpr const char* kBrowserNotMounted = "SD-карта не найдена.";
inline constexpr const char* kBrowserOpenDirFailed = "Не удалось открыть папку.";
inline constexpr const char* kBrowserInvalidName = "Недопустимое имя файла.";
inline constexpr const char* kBrowserNoSelection = "Сначала выберите файл.";
inline constexpr const char* kBrowserNotFound = "Файл не найден.";
inline constexpr const char* kBrowserFileExists = "Файл уже существует.";
inline constexpr const char* kBrowserPathTooLong = "Слишком длинное имя или путь.";
inline constexpr const char* kBrowserOpenProtected = "Нельзя удалить открытый файл.";
inline constexpr const char* kBrowserRemoveFailed = "Не удалось удалить файл.";
inline constexpr const char* kStorageError = "Ошибка работы с носителем.";

/* --- Статусы MConsole --- */
inline constexpr const char* kConsoleNoShowOpen = "Файл ещё не открыт.\rСохраните его как новый.";
inline constexpr const char* kConsoleTemplateProtected = "Шаблон сохранить нельзя.";
inline constexpr const char* kConsoleBadMagic = "Это не файл шоу.";
inline constexpr const char* kConsoleBadVersion = "Версия файла не поддерживается.";
inline constexpr const char* kConsoleBadCrc = "Контрольная сумма не совпала.";
inline constexpr const char* kConsoleBadLayout = "Файл шоу повреждён.";
inline constexpr const char* kConsoleTruncated = "Файл шоу обрезан или неполон.";
inline constexpr const char* kConsoleMissingGrup = "В файле нет ни одной группы.";
inline constexpr const char* kConsoleBadGroups = "Данные групп в файле повреждены.";
inline constexpr const char* kConsoleInvalidGroup = "Нельзя изменить эту группу.";
inline constexpr const char* kConsoleNoSelection = "Сначала выберите лебёдки.";
inline constexpr const char* kConsoleGroupOccupied = "В группе уже есть данные.";

/* --- Блокировка (%s = kWinchMark, %u = номер с 1, %s = список) --- */
/**
 * Маркер номера лебёдки. «№» (U+2116) нет в KOI8-R/шрифте панели → «?».
 * Используем ASCII «#».
 */
inline constexpr const char* kWinchMark = "#";
inline constexpr const char* kBlockByManualFmt =
    "Лебёдка %s%u заблокирована в ручном режиме.";
inline constexpr const char* kBlockByGroupsFmt =
    "Лебёдка %s%u заблокирована в группах: %s";
inline constexpr const char* kBlockSharedWinchesFmt =
    "Внимание: пересечение с заблокированными группами!\rЛебёдки: %s";

} // namespace uiMsg
