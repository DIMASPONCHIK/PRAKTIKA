/******************************************************************************
*                      КАФЕДРА №304 1 КУРС ПРОГИНЖ                            *
*                           Летняя Практика                                   *
*-----------------------------------------------------------------------------*
* Project Type  : Win32 Console Application                                   *
* Project Name  : PRAKTIKA                                                    *
* File Name     : PRAKTIKA.cpp                                                *
* Language      : C/C++                                                       *
* Programmer    : Букреев Дмитрий                                             *
* Created       : 20/05/26                                                    *
* Last Revision : 03/06/26                                                    *
* Comment(s)    : Индексная сортировка методом выбора записей о посадке       *
*                 самолётов. Контроль времени и бортового номера,             *
*                 проверка смысловых дублей, динамическая память.             *
******************************************************************************/

#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <cstring>
#include <cctype>
#include <cstdlib>
#include <clocale>
#include <fstream>
#include <iostream>

// ======================== ГЛОБАЛЬНЫЕ КОНСТАНТЫ ========================

const int MAX_TIME_LEN = 6;        // "HH:MM" + '\0'
const int MAX_TAIL_LEN = 8;        // UTF-8 "Б-1234" (7 байт) + '\0'
const int MAX_CITY_LEN = 100;      // достаточно для любого города в UTF-8
const int MAX_LINE_LEN = 200;      // максимальная длина строки в файле
const int MAX_ERROR_MSG = 500;     // максимальная длина сообщения об ошибке

// Глобальный массив имён тестовых файлов (вынесен в глобальную константу)
const char* TEST_FILES[] = {
    "test_pos1.txt", "test_pos2.txt", "test_pos3.txt", "test_pos4.txt", "test_pos5.txt",
    "test_pos6.txt", "test_pos7.txt", "test_pos8.txt", "test_pos9.txt", "test_pos10.txt",
    "test_neg1.txt", "test_neg2.txt", "test_neg3.txt", "test_neg4.txt", "test_neg5.txt",
    "test_neg6.txt", "test_neg7.txt", "test_neg8.txt", "test_neg9.txt", "test_neg10.txt"
};
const int NUM_TEST_FILES = sizeof(TEST_FILES) / sizeof(TEST_FILES[0]);

// Структура для хранения записи о самолёте
struct AircraftRecord {
    char time[MAX_TIME_LEN];        // время посадки "HH:MM"
    int flightNum;                  // номер рейса
    char tailNumber[MAX_TAIL_LEN];  // бортовой номер "Б-1234" (UTF-8)
    char destination[MAX_CITY_LEN]; // пункт отправления (UTF-8)
    bool timeValid;                 // true, если время корректно
    bool tailValid;                 // true, если бортовой номер корректен
};

// ======================== ПРОТОТИПЫ ФУНКЦИЙ С КОММЕНТАРИЯМИ ========================

/**
 * @brief Проверяет строку времени на соответствие формату "HH:MM" и допустимые значения.
 * @param timeStr Входная строка времени
 * @param minutes [выход] Количество минут от полуночи (при успехе)
 * @param valid [выход] true, если время корректно, иначе false
 * @return true, если разбор выполнен успешно (формат верен), иначе false
 */
bool parseTime(const char* timeStr, int& minutes, bool& valid);

/**
 * @brief Проверяет бортовой номер на соответствие формату "Б-XXXX" (русская 'Б' в UTF-8, дефис, 4 цифры).
 * @param tail Строка с бортовым номером
 * @return true, если номер корректен, иначе false
 */
bool validateTailNumber(const char* tail);

/**
 * @brief Читает данные из файла, выделяет динамическую память под массив записей.
 *        При ошибках времени или бортового номера записывает сообщения в errors.
 * @param filename Имя файла
 * @param records [выход] Указатель на массив записей (выделяется внутри)
 * @param count [выход] Количество прочитанных записей
 * @param errors Массив для сообщений об ошибках
 * @param errorCount [вход/выход] Текущее количество ошибок
 */
void readData(const char* filename, AircraftRecord*& records, int& count,
    char errors[][MAX_ERROR_MSG], int& errorCount);

/**
 * @brief Проверяет смысловые дубли: одинаковые бортовые номера в одно и то же время,
 *        а также одинаковые номера рейсов в одно и то же время.
 * @param records Массив записей
 * @param count Количество записей
 * @param errors Массив для сообщений об ошибках
 * @param errorCount [вход/выход] Счётчик ошибок
 */
void checkDuplicates(AircraftRecord* records, int count,
    char errors[][MAX_ERROR_MSG], int& errorCount);

/**
 * @brief Индексная сортировка методом выбора по времени посадки.
 *        Записи с некорректным временем помещаются в начало.
 * @param records Массив записей (не изменяется)
 * @param indices [выход] Массив индексов в отсортированном порядке
 * @param count Количество записей
 */
void selectionSortIndices(AircraftRecord* records, int* indices, int count);

/**
 * @brief Выводит таблицу записей в порядке, заданном массивом индексов.
 * @param records Массив записей
 * @param indices Отсортированные индексы
 * @param count Количество записей
 */
void printTable(AircraftRecord* records, int* indices, int count);

/**
 * @brief Освобождает динамически выделенную память.
 * @param records Указатель на массив записей
 * @param indices Указатель на массив индексов
 */
void freeMemory(AircraftRecord* records, int* indices);

// ======================== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ========================

/**
 * @brief Возвращает количество видимых символов в UTF-8 строке (не байт).
 * @param str Входная строка в UTF-8
 * @return Количество символов (не байт)
 * @note  Функция корректно обрабатывает многобайтовые последовательности UTF-8:
 *        символы от 0x00 до 0x7F – 1 байт, от 0xC2 до 0xDF – 2 байта,
 *        от 0xE0 до 0xEF – 3 байта, от 0xF0 до 0xF7 – 4 байта.
 */
int utf8_strlen(const char* str) {
    int len = 0;
    while (*str) {
        unsigned char c = static_cast<unsigned char>(*str);
        if (c < 0x80) { ++len; ++str; }
        else if ((c & 0xE0) == 0xC0) { ++len; str += 2; }
        else if ((c & 0xF0) == 0xE0) { ++len; str += 3; }
        else if ((c & 0xF8) == 0xF0) { ++len; str += 4; }
        else { ++len; ++str; } // ошибочная последовательность, считаем как один символ
    }
    return len;
}

/**
 * @brief Выводит строку в поле фиксированной видимой ширины (с учётом UTF-8).
 * @param str Выводимая строка
 * @param width Ширина поля в символах (не байтах)
 * @note  Функция вычисляет, сколько байт строки занимают первые 'width' символов,
 *        затем добавляет необходимое количество пробелов, чтобы общая видимая ширина
 *        стала равна 'width'.
 */
void printField(const char* str, int width) {
    // Ищем позицию в байтах, где заканчиваются 'width' символов
    const char* end = str;
    int seen = 0;
    while (*end && seen < width) {
        unsigned char c = static_cast<unsigned char>(*end);
        if (c < 0x80) { ++end; ++seen; }
        else if ((c & 0xE0) == 0xC0) { end += 2; ++seen; }
        else if ((c & 0xF0) == 0xE0) { end += 3; ++seen; }
        else if ((c & 0xF8) == 0xF0) { end += 4; ++seen; }
        else { ++end; ++seen; }
    }

    size_t bytes = end - str;
    char* temp = new char[bytes + 1];
    strncpy(temp, str, bytes);
    temp[bytes] = '\0';

    int visualLen = utf8_strlen(temp);
    int padBytes = width - visualLen;   // сколько пробелов добавить
    int byteWidth = static_cast<int>(bytes) + padBytes;

    printf("%-*s", byteWidth, temp);
    delete[] temp;
}

// ======================== ОСНОВНЫЕ ФУНКЦИИ ========================

bool parseTime(const char* timeStr, int& minutes, bool& valid) {
    valid = false;
    if (strlen(timeStr) != 5) return false;
    if (timeStr[2] != ':') return false;
    for (int i = 0; i < 5; ++i) {
        if (i == 2) continue;
        if (!isdigit(static_cast<unsigned char>(timeStr[i]))) return false;
    }
    int hh = (timeStr[0] - '0') * 10 + (timeStr[1] - '0');
    int mm = (timeStr[3] - '0') * 10 + (timeStr[4] - '0');
    if (hh >= 0 && hh <= 23 && mm >= 0 && mm <= 59) {
        minutes = hh * 60 + mm;
        valid = true;
        return true;
    }
    return false;
}

bool validateTailNumber(const char* tail) {
    if (strlen(tail) != 7) return false;
    if (static_cast<unsigned char>(tail[0]) != 0xD0 ||
        static_cast<unsigned char>(tail[1]) != 0x91) return false;
    if (tail[2] != '-') return false;
    for (int i = 3; i < 7; ++i) {
        if (!isdigit(static_cast<unsigned char>(tail[i]))) return false;
    }
    return true;
}

void readData(const char* filename, AircraftRecord*& records, int& count,
    char errors[][MAX_ERROR_MSG], int& errorCount) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        fprintf(stderr, "Error: cannot open file %s\n", filename);
        count = 0;
        records = nullptr;
        return;
    }

    char bom[3] = { 0 };
    file.read(bom, 3);
    bool hasBom = (file.gcount() == 3 &&
        static_cast<unsigned char>(bom[0]) == 0xEF &&
        static_cast<unsigned char>(bom[1]) == 0xBB &&
        static_cast<unsigned char>(bom[2]) == 0xBF);
    if (!hasBom) {
        file.clear();
        file.seekg(0, std::ios::beg);
    }

    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    char* buffer = new char[fileSize + 2];
    file.read(buffer, fileSize);
    file.close();
    buffer[fileSize] = '\0';

    size_t pos = 0;
    int lineCount = 0;
    for (size_t i = 0; i < fileSize; ++i)
        if (buffer[i] == '\n') ++lineCount;
    if (fileSize > 0 && buffer[fileSize - 1] != '\n') ++lineCount;

    records = new AircraftRecord[lineCount];
    count = 0;
    int lineNum = 0;

    while (pos < fileSize) {
        char rawLine[MAX_LINE_LEN];
        int rawIdx = 0;
        while (pos < fileSize && buffer[pos] != '\n' && rawIdx < MAX_LINE_LEN - 1)
            rawLine[rawIdx++] = buffer[pos++];
        if (pos < fileSize && buffer[pos] == '\n') ++pos;
        rawLine[rawIdx] = '\0';
        if (rawIdx > 0 && rawLine[rawIdx - 1] == '\r') rawLine[rawIdx - 1] = '\0';

        ++lineNum;
        if (rawLine[0] == '\0') continue;

        char* p = rawLine;
        while (*p == ' ') ++p;
        if (*p == '\0') continue;

        // Время
        char timeStr[MAX_TIME_LEN];
        int i = 0;
        while (*p && *p != ' ' && i < MAX_TIME_LEN - 1) timeStr[i++] = *p++;
        timeStr[i] = '\0';
        while (*p == ' ') ++p;

        // Номер рейса
        int flight = -1;
        if (isdigit(static_cast<unsigned char>(*p))) {
            flight = 0;
            while (isdigit(static_cast<unsigned char>(*p))) {
                flight = flight * 10 + (*p - '0');
                ++p;
            }
        }
        else {
            while (*p && *p != ' ') ++p;
        }
        while (*p == ' ') ++p;

        // Бортовой номер (исправленный парсинг)
        const char* tailStart = p;
        while (*p && *p != ' ') ++p;
        size_t tailWordLen = p - tailStart;
        p = const_cast<char*>(tailStart);

        char tail[MAX_TAIL_LEN];
        bool tailValidFlag = false;
        char originalTail[50];
        strncpy(originalTail, p, tailWordLen < 50 ? tailWordLen : 49);
        originalTail[tailWordLen < 50 ? tailWordLen : 49] = '\0';

        // Допустимая длина слова для бортового номера: 7 байт (2+1+4)
        if (tailWordLen == 7) {
            for (size_t j = 0; j < tailWordLen; ++j)
                tail[j] = *p++;
            tail[7] = '\0';
            tailValidFlag = validateTailNumber(tail);
        }
        else {
            while (*p && *p != ' ') ++p;
        }
        while (*p == ' ') ++p;

        if (!tailValidFlag) {
            tail[0] = '\0'; // помечаем как невалидный (будет показано "-----")
            snprintf(errors[errorCount++], MAX_ERROR_MSG,
                "Line %d: invalid tail number '%s' (must be '\xD0\x91-XXXX')", lineNum, originalTail);
        }

        // Город (оставшаяся часть строки)
        char city[MAX_CITY_LEN];
        i = 0;
        while (*p && i < MAX_CITY_LEN - 1) city[i++] = *p++;
        city[i] = '\0';

        int minutes;
        bool timeValidFlag;
        parseTime(timeStr, minutes, timeValidFlag);

        if (!timeValidFlag) {
            snprintf(errors[errorCount++], MAX_ERROR_MSG,
                "Line %d: invalid time '%s' (format HH:MM, 00-23, 00-59)", lineNum, timeStr);
        }

        strncpy(records[count].time, timeStr, MAX_TIME_LEN);
        records[count].time[MAX_TIME_LEN - 1] = '\0';
        records[count].flightNum = flight;
        strncpy(records[count].tailNumber, tail, MAX_TAIL_LEN);
        records[count].tailNumber[MAX_TAIL_LEN - 1] = '\0';
        strncpy(records[count].destination, city, MAX_CITY_LEN);
        records[count].destination[MAX_CITY_LEN - 1] = '\0';
        records[count].timeValid = timeValidFlag;
        records[count].tailValid = tailValidFlag;

        ++count;
    }
    delete[] buffer;
}

void checkDuplicates(AircraftRecord* records, int count,
    char errors[][MAX_ERROR_MSG], int& errorCount) {
    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ++j) {
            // Повтор бортового номера ТОЛЬКО если время одинаковое (и оба времени корректны)
            // Это соответствует ТЗ: один самолёт не может быть в двух местах одновременно,
            // но может садиться в разное время.
            if (records[i].timeValid && records[j].timeValid &&
                strcmp(records[i].tailNumber, records[j].tailNumber) == 0 &&
                strcmp(records[i].time, records[j].time) == 0) {
                snprintf(errors[errorCount++], MAX_ERROR_MSG,
                    "Semantic error: tail number '%s' at time %s is duplicated (records %d and %d)",
                    records[i].tailNumber, records[i].time, i + 1, j + 1);
            }
            // Одинаковый номер рейса в одно и то же время (только если оба времени корректны)
            if (records[i].timeValid && records[j].timeValid &&
                records[i].flightNum == records[j].flightNum &&
                strcmp(records[i].time, records[j].time) == 0) {
                snprintf(errors[errorCount++], MAX_ERROR_MSG,
                    "Semantic error: flight %d at %s is duplicated (records %d and %d)",
                    records[i].flightNum, records[i].time, i + 1, j + 1);
            }
        }
    }
}

void selectionSortIndices(AircraftRecord* records, int* indices, int count) {
    // Инициализация индексов
    for (int i = 0; i < count; ++i) indices[i] = i;

    // Лямбда-функция для сравнения двух записей по времени посадки
    // Возвращает true, если запись aIdx должна идти раньше, чем bIdx
    auto less = [&](int aIdx, int bIdx) -> bool {
        const AircraftRecord& ra = records[aIdx];
        const AircraftRecord& rb = records[bIdx];
        // Записи с некорректным временем помещаются в начало (они считаются "меньшими")
        if (!ra.timeValid && rb.timeValid) return true; 
        if (ra.timeValid && !rb.timeValid) return false;
        if (!ra.timeValid && !rb.timeValid) return aIdx < bIdx;
        // Оба времени корректны – сравниваем в минутах от полуночи
        int minA, minB;
        bool vA, vB;
        parseTime(ra.time, minA, vA);
        parseTime(rb.time, minB, vB);
        if (minA != minB) return minA < minB;
        // При равенстве времени сохраняем порядок индексов (стабильность)
        return aIdx < bIdx;
        };

    // Алгоритм сортировки выбором (индексная версия)
    for (int i = 0; i < count - 1; ++i) {
        int minIdx = i;
        for (int j = i + 1; j < count; ++j) {
            if (less(indices[j], indices[minIdx])) minIdx = j;
        }
        if (minIdx != i) std::swap(indices[i], indices[minIdx]);
    }
}

void printTable(AircraftRecord* records, int* indices, int count) {
    const int W_INDEX = 3;
    const int W_TIME = 8;
    const int W_FLIGHT = 7;
    const int W_TAIL = 7;
    const int W_CITY = 25;

    auto line = [&](char left, char mid, char right) {
        printf("%c", left);
        for (int i = 0; i < W_INDEX + 2; ++i) printf("-");
        printf("%c", mid);
        for (int i = 0; i < W_TIME + 2; ++i) printf("-");
        printf("%c", mid);
        for (int i = 0; i < W_FLIGHT + 2; ++i) printf("-");
        printf("%c", mid);
        for (int i = 0; i < W_TAIL + 2; ++i) printf("-");
        printf("%c", mid);
        for (int i = 0; i < W_CITY + 2; ++i) printf("-");
        printf("%c\n", right);
        };

    line('+', '+', '+');
    printf("| "); printField("No", W_INDEX); printf(" | ");
    printField("Time", W_TIME); printf(" | ");
    printField("Flight", W_FLIGHT); printf(" | ");
    printField("Tail", W_TAIL); printf(" | ");
    printField("Destination", W_CITY); printf(" |\n");
    line('+', '+', '+');

    for (int i = 0; i < count; ++i) {
        int idx = indices[i];
        AircraftRecord& rec = records[idx];

        char noStr[12];
        snprintf(noStr, sizeof(noStr), "%d", i + 1);

        printf("| "); printField(noStr, W_INDEX); printf(" | ");

        if (rec.timeValid) printField(rec.time, W_TIME);
        else printField("------", W_TIME);
        printf(" | ");

        if (rec.flightNum != -1) {
            char flightStr[20];
            snprintf(flightStr, sizeof(flightStr), "%d", rec.flightNum);
            printField(flightStr, W_FLIGHT);
        }
        else {
            printField("---", W_FLIGHT);
        }
        printf(" | ");

        if (rec.tailValid) printField(rec.tailNumber, W_TAIL);
        else printField("-----", W_TAIL);
        printf(" | ");

        printField(rec.destination, W_CITY);
        printf(" |\n");
    }

    line('+', '+', '+');
}

void freeMemory(AircraftRecord* records, int* indices) {
    delete[] records;
    delete[] indices;
}

int main() {
    // Настройка консоли для корректного отображения UTF-8
#ifdef _WIN32
    system("chcp 65001 > nul");      // Windows: переключаем кодовую страницу на UTF-8
    setlocale(LC_ALL, ".UTF-8");
#else
    setlocale(LC_ALL, "en_US.UTF-8"); // Linux/Mac: устанавливаем UTF-8 локаль
#endif

    // Отключаем буферизацию stdout для немедленного вывода (полезно в некоторых средах)
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("===== Aircraft Landing Log (Variant 26) =====\n");
    printf("Sort by landing time (index selection sort)\n\n");

    // Цикл выбора файла: повторяем запрос, пока не будет выбран существующий файл
    while (true) {
        printf("Available test files (enter number 1..%d or name, e.g., pos1):\n", NUM_TEST_FILES);
        for (int i = 0; i < NUM_TEST_FILES; ++i) {
            printf("  %2d. %s\n", i + 1, TEST_FILES[i]);
        }
        printf("Your choice: ");
        fflush(stdout);

        char input[100];
        scanf("%99s", input);

        const char* chosenFile = nullptr;
        int num = atoi(input);
        if (num >= 1 && num <= NUM_TEST_FILES) {
            chosenFile = TEST_FILES[num - 1];
        }
        else {
            // Поиск по имени (без расширения .txt)
            for (int i = 0; i < NUM_TEST_FILES; ++i) {
                char nameNoExt[100];
                strncpy(nameNoExt, TEST_FILES[i], sizeof(nameNoExt));
                nameNoExt[sizeof(nameNoExt) - 1] = '\0';
                char* dot = strchr(nameNoExt, '.');
                if (dot) *dot = '\0';
                if (strcmp(input, nameNoExt) == 0) {
                    chosenFile = TEST_FILES[i];
                    break;
                }
            }
        }

        if (chosenFile == nullptr) {
            printf("Invalid name or number. Try again.\n\n");
            continue;
        }

        std::ifstream test(chosenFile);
        if (!test.is_open()) {
            printf("File %s not found. Make sure the file is in the current directory.\n\n", chosenFile);
            continue;
        }
        test.close();

        printf("Selected file: %s\n\n", chosenFile);

        AircraftRecord* records = nullptr;
        int recordCount = 0;
        char errors[100][MAX_ERROR_MSG];
        int errorCount = 0;

        readData(chosenFile, records, recordCount, errors, errorCount);
        if (recordCount == 0) {
            fprintf(stderr, "No data to process.\n");
            freeMemory(records, nullptr);
            return 1;
        }

        checkDuplicates(records, recordCount, errors, errorCount);

        int* indices = new int[recordCount];
        selectionSortIndices(records, indices, recordCount);

        if (errorCount > 0) {
            printf("\n=== ERRORS FOUND ===\n");
            for (int i = 0; i < errorCount; ++i) {
                printf("%s\n", errors[i]);
            }
            printf("\n");
        }
        else {
            printf("\nNo errors found.\n\n");
        }

        printTable(records, indices, recordCount);

        freeMemory(records, indices);
        break; // успешное завершение
    }
    return 0;
}