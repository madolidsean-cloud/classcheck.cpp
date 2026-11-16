#define UNICODE
#define _UNICODE
#include <windows.h>
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

namespace {

constexpr int ID_NAME_EDIT = 101;
constexpr int ID_ADD_BUTTON = 102;
constexpr int ID_LIST = 103;
constexpr int ID_PRESENT_BUTTON = 104;
constexpr int ID_ABSENT_BUTTON = 105;
constexpr int ID_SAVE_BUTTON = 106;
constexpr int ID_STATUS = 107;

const wchar_t* WINDOW_CLASS = L"SimpleAttendanceCheckerWindow";
const char* DATA_FILE = "attendance.csv";

struct Student {
    std::wstring name;
    bool present = false;
};

std::vector<Student> students;
HWND nameEdit = nullptr;
HWND studentList = nullptr;
HWND statusLabel = nullptr;
HWND mainWindow = nullptr;

std::string toUtf8(const std::wstring& text) {
    if (text.empty()) return std::string();
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1) return std::string();
    std::string result(static_cast<std::size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

std::wstring fromUtf8(const std::string& text) {
    if (text.empty()) return std::wstring();
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (size <= 1) return std::wstring();
    std::wstring result(static_cast<std::size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &result[0], size);
    return result;
}

void updateStatus() {
    int presentCount = 0;
    for (const Student& student : students) {
        if (student.present) ++presentCount;
    }

    const std::wstring message = L"Students: " + std::to_wstring(students.size()) +
        L"    Present: " + std::to_wstring(presentCount) +
        L"    Absent: " + std::to_wstring(students.size() - presentCount);
    SetWindowTextW(statusLabel, message.c_str());
}

void refreshList(int selectedIndex = -1) {
    SendMessageW(studentList, LB_RESETCONTENT, 0, 0);
    for (const Student& student : students) {
        const std::wstring row = student.name + (student.present ? L" - Present" : L" - Absent");
        SendMessageW(studentList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(row.c_str()));
    }
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(students.size())) {
        SendMessageW(studentList, LB_SETCURSEL, selectedIndex, 0);
    }
    updateStatus();
}

bool saveAttendance() {
    std::ofstream file(DATA_FILE, std::ios::binary | std::ios::trunc);
    if (!file) return false;

    file << "Name,Status\n";
    for (const Student& student : students) {
        file << toUtf8(student.name) << ',' << (student.present ? "Present" : "Absent") << '\n';
    }
    return static_cast<bool>(file);
}

void loadAttendance() {
    std::ifstream file(DATA_FILE, std::ios::binary);
    if (!file) return;

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line == "Name,Status") continue;

        const std::size_t comma = line.rfind(',');
        if (comma == std::string::npos) continue;
        const std::string name = line.substr(0, comma);
        if (name.empty()) continue;

        Student student;
        student.name = fromUtf8(name);
        student.present = line.substr(comma + 1) == "Present";
        students.push_back(student);
    }
}

void showSaveError() {
    MessageBoxW(mainWindow,
        L"Could not save attendance.csv. Check that the application folder is writable.",
        L"Save error", MB_OK | MB_ICONERROR);
}

void setAttendance(bool present) {
    const LRESULT selection = SendMessageW(studentList, LB_GETCURSEL, 0, 0);
    if (selection == LB_ERR) {
        MessageBoxW(mainWindow, L"Select a student first.", L"No student selected", MB_OK | MB_ICONINFORMATION);
        return;
    }

    const int index = static_cast<int>(selection);
    students[index].present = present;
    refreshList(index);
    if (!saveAttendance()) showSaveError();
}

void layoutControls(HWND window) {
    RECT client;
    GetClientRect(window, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int margin = 16;
    const int buttonWidth = 112;
    const int gap = 8;

    MoveWindow(nameEdit, margin, margin, width - margin * 3 - buttonWidth, 28, TRUE);
    MoveWindow(GetDlgItem(window, ID_ADD_BUTTON), width - margin - buttonWidth, margin, buttonWidth, 28, TRUE);
    MoveWindow(studentList, margin, margin + 40, width - margin * 2, height - 40 - 90, TRUE);

    const int buttonY = height - 82;
    MoveWindow(GetDlgItem(window, ID_PRESENT_BUTTON), margin, buttonY, buttonWidth, 30, TRUE);
    MoveWindow(GetDlgItem(window, ID_ABSENT_BUTTON), margin + buttonWidth + gap, buttonY, buttonWidth, 30, TRUE);
    MoveWindow(GetDlgItem(window, ID_SAVE_BUTTON), width - margin - buttonWidth, buttonY, buttonWidth, 30, TRUE);
    MoveWindow(statusLabel, margin, height - 42, width - margin * 2, 24, TRUE);
}

LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        nameEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0,
            window, reinterpret_cast<HMENU>(ID_NAME_EDIT), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Add student",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
            window, reinterpret_cast<HMENU>(ID_ADD_BUTTON), nullptr, nullptr);
        studentList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS,
            0, 0, 0, 0, window, reinterpret_cast<HMENU>(ID_LIST), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Mark present",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
            window, reinterpret_cast<HMENU>(ID_PRESENT_BUTTON), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Mark absent",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
            window, reinterpret_cast<HMENU>(ID_ABSENT_BUTTON), nullptr, nullptr);
        CreateWindowW(L"BUTTON", L"Save now",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0,
            window, reinterpret_cast<HMENU>(ID_SAVE_BUTTON), nullptr, nullptr);
        statusLabel = CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0,
            window, reinterpret_cast<HMENU>(ID_STATUS), nullptr, nullptr);

        loadAttendance();
        refreshList();
        layoutControls(window);
        return 0;
    }
    case WM_SIZE:
        if (nameEdit && studentList && statusLabel) layoutControls(window);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_ADD_BUTTON: {
            const int length = GetWindowTextLengthW(nameEdit);
            if (length <= 0) {
                MessageBoxW(window, L"Type a student name first.", L"Name required", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            std::wstring name(static_cast<std::size_t>(length) + 1, L'\0');
            GetWindowTextW(nameEdit, &name[0], length + 1);
            name.resize(static_cast<std::size_t>(length));
            const std::size_t first = name.find_first_not_of(L" \t");
            if (first == std::wstring::npos) {
                MessageBoxW(window, L"Type a student name first.", L"Name required", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            name = name.substr(first, name.find_last_not_of(L" \t") - first + 1);
            if (name.find_first_of(L",\r\n") != std::wstring::npos) {
                MessageBoxW(window, L"Student names cannot contain commas or line breaks.", L"Invalid name", MB_OK | MB_ICONWARNING);
                return 0;
            }
            const bool alreadyExists = std::any_of(students.begin(), students.end(),
                [&name](const Student& student) { return student.name == name; });
            if (alreadyExists) {
                MessageBoxW(window, L"That student is already on the list.", L"Duplicate student", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            students.push_back({name, false});
            SetWindowTextW(nameEdit, L"");
            refreshList(static_cast<int>(students.size()) - 1);
            if (!saveAttendance()) showSaveError();
            SetFocus(nameEdit);
            return 0;
        }
        case ID_PRESENT_BUTTON:
            setAttendance(true);
            return 0;
        case ID_ABSENT_BUTTON:
            setAttendance(false);
            return 0;
        case ID_SAVE_BUTTON:
            if (saveAttendance()) {
                MessageBoxW(window, L"Attendance saved to attendance.csv.", L"Saved", MB_OK | MB_ICONINFORMATION);
            } else {
                showSaveError();
            }
            return 0;
        case ID_LIST:
            if (HIWORD(wParam) == LBN_DBLCLK) setAttendance(true);
            return 0;
        }
        break;
    case WM_DESTROY:
        if (!saveAttendance()) showSaveError();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand) {
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = WINDOW_CLASS;

    if (!RegisterClassExW(&windowClass)) {
        MessageBoxW(nullptr, L"Could not register the application window.", L"Startup error", MB_OK | MB_ICONERROR);
        return 1;
    }

    mainWindow = CreateWindowExW(0, WINDOW_CLASS, L"Simple Attendance Checker",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 620, 460,
        nullptr, nullptr, instance, nullptr);
    if (!mainWindow) {
        MessageBoxW(nullptr, L"Could not create the application window.", L"Startup error", MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(mainWindow, showCommand);
    UpdateWindow(mainWindow);

    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
