// Тема 11: программы для работы с файлами. Вариант: "Личный бюджет".
//
// Тип: IN (доход) или OUT (расход). 

#include <iostream>   // подключаю ввод/вывод в консоль
#include <fstream>    // тут работа с файлами (ifstream/ofstream)
#include <string>     // строковый тип std::string
#include <vector>     // динамический массив (вектор)
#include <map>        // ассоциативный массив (словарь категория -> сумма)
#include <limits>     // чтобы чистить ввод (numeric_limits)
#include <algorithm>  // для transform
#include <cctype>     // для toupper, isspace

// ===== ДОБАВЛЕНО: для корректного русского вывода/ввода и безопасной конвертации =====
#include <locale>
#include <io.h>
#include <fcntl.h>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>   // будем конвертировать UTF-8/CP1251 <-> UTF-16 без падений
#endif

using namespace std;  // чтобы не писать std:: перед каждым словом

const string FILE_NAME = "budget.txt"; // имя файла с данными

// Вспомогательные утилиты для нормализации ввода 
// Убираю пробелы по краям
static inline string trim(const string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}
// Перевожу строку в ВЕРХНИЙ регистр (для сравнения IN/OUT)
static inline string toUpper(string s) {
    transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(toupper(c)); });
    return s;
}

// безопасная конвертация между UTF-8/CP1251 и wide ======
static wstring bytes_to_w(const string& bytes) {
#ifdef _WIN32
    // 1) пробуем как UTF-8 с проверкой на ошибки
    int need = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        bytes.data(), (int)bytes.size(), nullptr, 0);
    if (need > 0) {
        wstring w(need, L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            bytes.data(), (int)bytes.size(), &w[0], need);
        return w;
    }
    // 2) fallback: пробуем CP1251 (русская Windows)
    need = MultiByteToWideChar(1251, 0, bytes.data(), (int)bytes.size(), nullptr, 0);
    if (need > 0) {
        wstring w(need, L'\0');
        MultiByteToWideChar(1251, 0, bytes.data(), (int)bytes.size(), &w[0], need);
        return w;
    }
#endif
    // 3) самый грубый fallback — побайтовое расширение (лучше, чем падать)
    return wstring(bytes.begin(), bytes.end());
}

static string w_to_utf8(const wstring& ws) {
#ifdef _WIN32
    int need = WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
    string out(need, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.data(), (int)ws.size(), &out[0], need, nullptr, nullptr);
    return out;
#else
    // на *nix обычно и так UTF-8, но делаем самый простой путь
    string out(ws.begin(), ws.end());
    return out;
#endif
}

// Снять UTF-8 BOM у первой строки (если вдруг есть)
static inline void strip_bom(string& s) {
    if (s.size() >= 3 &&
        (unsigned char)s[0] == 0xEF &&
        (unsigned char)s[1] == 0xBB &&
        (unsigned char)s[2] == 0xBF) {
        s.erase(0, 3);
    }
}

// Перевести стандартные потоки консоли в UTF-16 (Windows)
static void setup_console_utf16() {
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);
    ios::sync_with_stdio(false);
    wcin.tie(nullptr);
#endif
}

// Структура одной записи бюджета (как маленькая «табличная строка»)
struct Record {
    string date;      // дата: например 2025-10-16
    string kind;      // тип: IN или OUT
    string category;  // категория: Еда/Зарплата/Транспорт и т.п.
    double amount;    // сумма (положительная)
    string note;      // произвольный комментарий
};

// Простой разбор строки по разделителю ';'
vector<string> split(const string& s, char sep) { // s — исходная строка, sep — чем режем
    vector<string> res;                           // сюда складываю куски
    string cur;                                   // текущий набираемый кусок
    for (char c : s) {                            // прохожусь по всем символам строки
        if (c == sep) {                           // если встретился разделитель
            res.push_back(cur);                   // добавляю накопленное
            cur.clear();                          // и начинаю новый кусок
        }
        else cur.push_back(c);                  // иначе просто дописываю символ
    }
    res.push_back(cur);                           // докидываю последний кусок
    return res;                                   // возвращаю массив строк
}

// Преобразую запись Record в строку формата "дата;тип;категория;сумма;коммент"
string toLine(const Record& r) {
    return r.date + ";" + r.kind + ";" + r.category + ";" +
        to_string(r.amount) + ";" + r.note;    // to_string — перевожу число в текст
}

// Обратная операция: из строки делаю Record. Если что-то не так — верну false.
bool fromLine(const string& line, Record& out) {
    auto parts = split(line, ';');                // режу строку на кусочки
    if (parts.size() < 5) return false;           // если кусков меньше 5 — формат сломан
    out.date = trim(parts[0]);                    // раскладываю по полям
    out.kind = toUpper(trim(parts[1]));           // НОРМАЛИЗУЮ ТИП -> IN/OUT
    out.category = trim(parts[2]);
    try {
        out.amount = stod(parts[3]);              // stod — превращает текст в число (double)
    }
    catch (...) { return false; }               // если не получилось — бросаю false
    out.note = parts[4];
    return true;                                  // всё хорошо
}

// Пункт меню: добавить одну запись и сохранить её в файл
void addRecord() {
    Record r;                                     // создаю пустую запись

    // используем широкие потоки для консоли, а в файл кладём UTF-8
    wcout << L"Дата (напр. 2025-10-16): ";          // прошу ввести дату
    wcin.ignore(numeric_limits<streamsize>::max(), L'\n'); // чищу буфер ввода (после ввода числа)
    wstring wtmp;
    getline(wcin, wtmp);
    r.date = trim(w_to_utf8(wtmp));                // читаю строку целиком (с пробелами)

    wcout << L"Тип (IN=доход, OUT=расход): ";       // тип операции
    getline(wcin, wtmp);
    r.kind = toUpper(trim(w_to_utf8(wtmp)));        // НОРМАЛИЗАЦИЯ РЕГИСТРА
    if (r.kind != "IN" && r.kind != "OUT") {        // простая валидация
        wcout << L"Тип должен быть IN или OUT. Запись отменена.\n";
        return;
    }

    wcout << L"Категория (еда/транспорт/зарплата/...): "; // категория
    getline(wcin, wtmp);
    r.category = trim(w_to_utf8(wtmp));

    wcout << L"Сумма: ";                            // сумма текстом (чтобы потом проверить)
    getline(wcin, wtmp);
    try { r.amount = stod(w_to_utf8(wtmp)); }       // пробую превратить в число
    catch (...) {
        wcout << L"Сумма странная, записывать не буду.\n";
        return;
    }

    wcout << L"Комментарий: ";                      // комментарий к записи
    getline(wcin, wtmp);
    r.note = w_to_utf8(wtmp);

    ofstream fout(FILE_NAME, ios::app | ios::binary); // открываю файл на дозапись (append, UTF-8)
    if (!fout.is_open()) {                        // если вдруг не открылся
        wcout << L"Не могу открыть файл для записи :(\n";
        return;
    }
    fout << toLine(r) << "\n";                    // пишу строку и перевод строки
    fout.close();                                 // закрываю файл (на всякий случай)
    wcout << L"Записал.\n";                       // подтверждение для пользователя
}

// Читаю весь файл и возвращаю все записи как вектор
vector<Record> readAll() {
    vector<Record> v;                             // сюда сложу всё, что прочитал
    ifstream fin(FILE_NAME, ios::in | ios::binary); // открываю файл на чтение
    if (!fin.is_open()) return v;                 // если файла нет — верну пустой список
    string line;                                  // переменная для строки из файла (сырые байты)
    bool first = true;
    while (getline(fin, line)) {                  // читаю построчно
        if (first) { strip_bom(line); first = false; } // снять возможный BOM
        if (line.empty()) continue;               // пустые строки пропускаю
        Record r;                                 // сюда попробую распарсить
        if (fromLine(line, r)) v.push_back(r);    // если формат ок — кладу в вектор
        // иначе просто тихо пропускаю битую строку
    }
    return v;                                     // отдаю все записи
}

// Показать всё, что есть в файле, в аккуратном виде
void showAll() {
    auto v = readAll();                           // читаю весь файл
    if (v.empty()) {                              // если ничего нет
        wcout << L"Пока записей нет.\n";
        return;
    }
    wcout << L"=== Все операции ===\n";           // заголовок
    int i = 1;                                    // счётчик строк
    for (auto& r : v) {                           // пробегаюсь по всем записям
        wcout << i++ << L") "                     // номер
            << bytes_to_w(r.date) << L"  [" << bytes_to_w(r.kind) << L"]  " // дата и тип
            << bytes_to_w(r.category) << L" : "   // категория
            << r.amount << L"  // "               // сумма
            << bytes_to_w(r.note) << L"\n";       // комментарий
    }
}

// Аналитика: суммы доходов/расходов, по категориям, максимальный расход, поиск по дате
void analyze() {
    auto v = readAll();                           // читаю все записи
    wcout << L"=== Аналитика по файлу '" << bytes_to_w(FILE_NAME) << L"' ===\n";
    wcout << L"Всего операций: " << v.size() << L"\n";

    double totalIn = 0.0, totalOut = 0.0;         // общие суммы доходов/расходов
    map<string, double> byCat;                    // словарь «категория -> сумма расходов»
    double maxOut = 0.0;                          // самый большой расход
    string maxOutDescr = "(нет)";                 // описание для максимального расхода

    for (auto& r : v) {                           // пробегаюсь по всем записям
        // ВАЖНО: r.kind уже нормализован к верхнему регистру в fromLine/addRecord
        if (r.kind == "IN") totalIn += r.amount;  // доход — прибавляю к totalIn
        else if (r.kind == "OUT") {               // расход — в отдельную ветку
            totalOut += r.amount;                 // суммирую общий расход
            byCat[r.category] += r.amount;        // накапливаю по категории
            if (r.amount > maxOut) {              // проверяю максимум
                maxOut = r.amount;                // обновляю сумму максимума
                maxOutDescr =                     // собираю текст-описание максимального расхода
                    r.date + " " + r.category + " (" + r.note + ")";
            }
        }
    }

    wcout << L"Сумма доходов:  " << totalIn << L"\n";                // печатаю итоги
    wcout << L"Сумма расходов: " << totalOut << L"\n";
    wcout << L"Баланс (IN-OUT): " << (totalIn - totalOut) << L"\n";  // баланс = доходы - расходы

    if (!byCat.empty()) {                                            // если есть расходы по категориям
        wcout << L"\nРасходы по категориям:\n";
        for (auto& kv : byCat) {                                     // kv.first — категория, kv.second — сумма
            wcout << L" - " << bytes_to_w(kv.first) << L": " << kv.second << L"\n";
        }
    }

    wcout << L"Самый большой расход: " << maxOut                      // печатаю максимум
        << L" -> " << bytes_to_w(maxOutDescr) << L"\n";

    // Небольшой поиск по дате: можно ввести целиком 2025-10-16 или кусок 2025-10
    wcout << L"\nПоиск по дате (введите часть даты или Enter, чтобы пропустить): ";
    wstring wneedle;                                                  // сюда прочитаю, что ищем
    wcin.ignore(numeric_limits<streamsize>::max(), L'\n');            // чищу буфер перед getline
    getline(wcin, wneedle);                                           // читаю строку поиска
    string needle = trim(w_to_utf8(wneedle));
    if (!needle.empty()) {                                            // если что-то ввели
        int cnt = 0;                                                  // счётчик найденных строк
        for (auto& r : v) {                                           // бегу по всем записям
            if (r.date.find(needle) != string::npos) {                // если подстрока нашлась в дате
                wcout << bytes_to_w(r.date) << L" [" << bytes_to_w(r.kind) << L"] "
                    << bytes_to_w(r.category) << L" : " << r.amount
                    << L" // " << bytes_to_w(r.note) << L"\n";
                cnt++;
            }
        }
        if (cnt == 0)                                                 // если ничего не нашли
            wcout << L"Ничего не нашёл по дате \"" << bytes_to_w(needle) << L"\"\n";
    }
}

// Убедиться, что файл существует: если нет — создать пустой
void ensureFile() {
    ifstream fin(FILE_NAME);                // пробую открыть на чтение
    if (!fin.good()) {                      // если не получилось (нет файла)
        ofstream fout(FILE_NAME, ios::binary); // просто создаю пустой файл
        // здесь ничего не пишу — файл сам появится при закрытии
    }
}

// Точка входа в программу
int main() {
    setup_console_utf16();                 // чтобы русские буквы нормально печатались
    ensureFile();                          // создаю файл, если его ещё нет

    while (true) {                          // бесконечный цикл меню, пока не выберут «0»
        wcout << L"\n==== Личный бюджет ====\n";                 // шапка меню
        wcout << L"1. Добавить операцию (запись)\n";             // пункт 1
        wcout << L"2. Показать все (чтение)\n";                  // пункт 2
        wcout << L"3. Анализ (доход/расход/категории/поиск)\n";  // пункт 3
        wcout << L"0. Выход\n";                                  // выход
        wcout << L"Ваш выбор: ";                                 // приглашение ввода

        int c;                                                   // сюда читаю номер пункта
        if (!(wcin >> c)) {                                      // если ввели не число
            wcin.clear();                                        // сбрасываю ошибку
            wcin.ignore(numeric_limits<streamsize>::max(), L'\n'); // вычищаю мусор из ввода
            wcout << L"Нужно число.\n";                          // подсказка пользователю
            continue;                                            // заново показываю меню
        }

        if (c == 1) addRecord();                                 // 1 — добавление записи
        else if (c == 2) showAll();                              // 2 — показать все записи
        else if (c == 3) analyze();                              // 3 — аналитика
        else if (c == 0) {                                       // 0 — выход из программы
            wcout << L"Пока!\n";
            break;                                               // выходим из while(true)
        }
        else wcout << L"Нет такого пункта.\n";                   // любое другое число — ошибка меню
    }

    return 0;                                                    // нормальное завершение программы
}
