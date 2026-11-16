#define UNICODE
#define _UNICODE
#include <windows.h>

#include <string>
#include <vector>

namespace {

constexpr int IdInput = 1001;
constexpr int NameInput = 1002;
constexpr int StatusChoice = 1003;
constexpr int StudentList = 1004;
constexpr int AddButton = 1005;
constexpr int MarkButton = 1006;
constexpr int RemoveButton = 1007;

struct Student {
    std::wstring id;
    std::wstring name;
    std::wstring status = L"Unmarked";
};

std::vector<Student> students;
HWND idInput = nullptr;
HWND nameInput = nullptr;
HWND statusChoice = nullptr;
HWND studentList = nullptr;

HWND addControl(HWND parent, const wchar_t *type, const wchar_t *text,
                DWORD style, int x, int y, int width, int height, int id) {
    HWND control = CreateWindowExW(
        0, type, text, WS_CHILD | WS_VISIBLE | style,
        x, y, width, height, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr), nullptr);
    SendMessageW(control, WM_SETFONT,
                 reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    return control;
}

std::wstring controlText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring value(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(control, value.data(), length + 1);
    value.resize(static_cast<size_t>(length));
    return value;
}

void refreshList() {
    SendMessageW(studentList, LB_RESETCONTENT, 0, 0);
    for (const Student &student : students) {
        const std::wstring row = student.id + L"    |    " + student.name + L"    |    " + student.status;
        SendMessageW(studentList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(row.c_str()));
    }
}

int selectedStudent() {
    return static_cast<int>(SendMessageW(studentList, LB_GETCURSEL, 0, 0));
}

void showNoSelectionMessage() {
    MessageBoxW(nullptr, L"Select a student from the list first.", L"ClassCheck",
                MB_OK | MB_ICONINFORMATION);
}

LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        addControl(window, L"STATIC", L"Student ID:", 0, 20, 20, 90, 24, 0);
        idInput = addControl(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL,
                             110, 18, 180, 26, IdInput);
        addControl(window, L"STATIC", L"Student name:", 0, 310, 20, 100, 24, 0);
        nameInput = addControl(window, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL,
                               410, 18, 270, 26, NameInput);

        addControl(window, L"STATIC", L"Attendance:", 0, 20, 62, 90, 24, 0);
        statusChoice = addControl(window, L"COMBOBOX", L"",
                                  CBS_DROPDOWNLIST | WS_VSCROLL, 110, 58, 180, 180, StatusChoice);
        SendMessageW(statusChoice, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Unmarked"));
        SendMessageW(statusChoice, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Present"));
        SendMessageW(statusChoice, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Absent"));
        SendMessageW(statusChoice, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Late"));
        SendMessageW(statusChoice, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Excused"));
        SendMessageW(statusChoice, CB_SETCURSEL, 0, 0);

        addControl(window, L"BUTTON", L"Add student", BS_PUSHBUTTON,
                   310, 58, 110, 30, AddButton);
        addControl(window, L"BUTTON", L"Set status", BS_PUSHBUTTON,
                   430, 58, 110, 30, MarkButton);
        addControl(window, L"BUTTON", L"Remove selected", BS_PUSHBUTTON,
                   550, 58, 130, 30, RemoveButton);

        addControl(window, L"STATIC", L"Students (ID | Name | Status):", 0,
                   20, 108, 300, 24, 0);
        studentList = addControl(window, L"LISTBOX", L"",
                                  WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                  20, 134, 660, 270, StudentList);
        return 0;
    }
    case WM_COMMAND: {
        const int controlId = LOWORD(wParam);
        if (controlId == AddButton) {
            const std::wstring id = controlText(idInput);
            const std::wstring name = controlText(nameInput);
            if (id.empty() || name.empty()) {
                MessageBoxW(window, L"Enter both a student ID and name.", L"Missing information",
                            MB_OK | MB_ICONWARNING);
                return 0;
            }
            for (const Student &student : students) {
                if (student.id == id) {
                    MessageBoxW(window, L"That student ID is already in the list.", L"Duplicate ID",
                                MB_OK | MB_ICONWARNING);
                    return 0;
                }
            }
            students.push_back({id, name, L"Unmarked"});
            SetWindowTextW(idInput, L"");
            SetWindowTextW(nameInput, L"");
            refreshList();
            return 0;
        }
        if (controlId == MarkButton) {
            const int index = selectedStudent();
            if (index == LB_ERR) {
                showNoSelectionMessage();
                return 0;
            }
            const int statusIndex = static_cast<int>(SendMessageW(statusChoice, CB_GETCURSEL, 0, 0));
            const LRESULT length = SendMessageW(statusChoice, CB_GETLBTEXTLEN, statusIndex, 0);
            std::wstring status(static_cast<size_t>(length) + 1, L'\0');
            SendMessageW(statusChoice, CB_GETLBTEXT, statusIndex,
                         reinterpret_cast<LPARAM>(status.data()));
            status.resize(static_cast<size_t>(length));
            students[static_cast<size_t>(index)].status = status;
            refreshList();
            SendMessageW(studentList, LB_SETCURSEL, index, 0);
            return 0;
        }
        if (controlId == RemoveButton) {
            const int index = selectedStudent();
            if (index == LB_ERR) {
                showNoSelectionMessage();
                return 0;
            }
            students.erase(students.begin() + index);
            refreshList();
            return 0;
        }
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    const wchar_t className[] = L"ClassCheckNativeWindow";
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = windowProcedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (!RegisterClassW(&windowClass)) {
        MessageBoxW(nullptr, L"Could not register the application window.", L"ClassCheck",
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    HWND window = CreateWindowExW(0, className, L"ClassCheck — Simple Attendance",
                                  WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                  720, 470, nullptr, nullptr, instance, nullptr);
    if (!window) {
        MessageBoxW(nullptr, L"Could not create the application window.", L"ClassCheck",
                    MB_OK | MB_ICONERROR);
        return 1;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
