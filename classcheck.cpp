#include <QApplication>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDate>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include <QByteArray>
#include <QVariant>

namespace {

const QStringList statuses{QStringLiteral("Unmarked"), QStringLiteral("Present"),
                           QStringLiteral("Absent"), QStringLiteral("Late"),
                           QStringLiteral("Excused")};

QString hashPassword(const QString &password, const QByteArray &salt) {
    QByteArray hash = salt + password.toUtf8();
    // Iterated salted hash for a lightweight local demo, not a substitute for
    // a production password-hashing scheme such as Argon2id or bcrypt.
    for (int i = 0; i < 150000; ++i)
        hash = QCryptographicHash::hash(hash, QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toHex());
}

bool runQuery(QSqlQuery &query, const QString &context) {
    if (query.exec()) return true;
    QMessageBox::critical(nullptr, QStringLiteral("Database error"),
                          context + QStringLiteral("\n\n") + query.lastError().text());
    return false;
}

bool openDatabase() {
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataDir.isEmpty() || !QDir().mkpath(dataDir)) {
        QMessageBox::critical(nullptr, QStringLiteral("Startup error"),
                              QStringLiteral("Could not create the application data folder."));
        return false;
    }
    const QString path = dataDir + QStringLiteral("/attendance.sqlite");
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    db.setDatabaseName(path);
    if (!db.open()) {
        QMessageBox::critical(nullptr, QStringLiteral("Database error"), db.lastError().text());
        return false;
    }
    QSqlQuery query;
    if (!runQuery(query, QStringLiteral("Could not initialize the database."))) return false;
    query.prepare(QStringLiteral("CREATE TABLE IF NOT EXISTS students ("
                                 "id TEXT PRIMARY KEY, name TEXT NOT NULL)"));
    if (!runQuery(query, QStringLiteral("Could not create the student table."))) return false;
    query.prepare(QStringLiteral("CREATE TABLE IF NOT EXISTS attendance ("
                                 "student_id TEXT NOT NULL, date TEXT NOT NULL, status TEXT NOT NULL, "
                                 "PRIMARY KEY(student_id, date), "
                                 "FOREIGN KEY(student_id) REFERENCES students(id) ON DELETE CASCADE)"));
    if (!runQuery(query, QStringLiteral("Could not create the attendance table."))) return false;
    query.prepare(QStringLiteral("CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY, value TEXT NOT NULL)"));
    return runQuery(query, QStringLiteral("Could not create the settings table."));
}

bool authenticateOrSetUp() {
    QSqlQuery query;
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key='password_salt'"));
    if (!runQuery(query, QStringLiteral("Could not read login settings."))) return false;
    const bool firstRun = !query.next();

    QDialog dialog;
    dialog.setWindowTitle(firstRun ? QStringLiteral("Create teacher login") : QStringLiteral("Teacher login"));
    dialog.setModal(true);
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    auto *password = new QLineEdit;
    password->setEchoMode(QLineEdit::Password);
    password->setPlaceholderText(firstRun ? QStringLiteral("Create a password") : QStringLiteral("Password"));
    form->addRow(QStringLiteral("Password:"), password);
    QLineEdit *confirm = nullptr;
    if (firstRun) {
        confirm = new QLineEdit;
        confirm->setEchoMode(QLineEdit::Password);
        confirm->setPlaceholderText(QStringLiteral("Enter it again"));
        form->addRow(QStringLiteral("Confirm:"), confirm);
        layout->addWidget(new QLabel(QStringLiteral("First run: set a teacher password for this computer.")));
    }
    layout->addLayout(form);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    for (;;) {
        if (dialog.exec() != QDialog::Accepted) return false;
        if (password->text().size() < 8) {
            QMessageBox::warning(&dialog, QStringLiteral("Password too short"),
                                 QStringLiteral("Use at least 8 characters."));
            continue;
        }
        if (firstRun && password->text() != confirm->text()) {
            QMessageBox::warning(&dialog, QStringLiteral("Passwords do not match"),
                                 QStringLiteral("Please enter the same password in both fields."));
            continue;
        }
        break;
    }

    if (firstRun) {
        const QByteArray salt = QCryptographicHash::hash(QByteArray::number(QDateTime::currentMSecsSinceEpoch()) +
                                    QByteArray::number(QRandomGenerator::global()->generate64()),
                                    QCryptographicHash::Sha256).toHex();
        QSqlQuery insert;
        insert.prepare(QStringLiteral("INSERT INTO settings(key,value) VALUES('password_salt',?),('password_hash',?)"));
        insert.addBindValue(QString::fromLatin1(salt));
        insert.addBindValue(hashPassword(password->text(), salt));
        return runQuery(insert, QStringLiteral("Could not save teacher login."));
    }

    const QString saltText = query.value(0).toString();
    QSqlQuery hashQuery;
    hashQuery.prepare(QStringLiteral("SELECT value FROM settings WHERE key='password_hash'"));
    if (!runQuery(hashQuery, QStringLiteral("Could not read teacher login.")) || !hashQuery.next()) return false;
    if (hashPassword(password->text(), QByteArray::fromLatin1(saltText.toLatin1())) != hashQuery.value(0).toString()) {
        QMessageBox::warning(nullptr, QStringLiteral("Login failed"), QStringLiteral("Incorrect password."));
        return false;
    }
    return true;
}

QString csvField(QString value) {
    value.replace('"', QStringLiteral("\"\""));
    return QStringLiteral("\"") + value + QStringLiteral("\"");
}

class AttendanceWindow final : public QWidget {
public:
    AttendanceWindow() {
        setWindowTitle(QStringLiteral("ClassCheck — Attendance"));
        resize(820, 560);

        auto *root = new QVBoxLayout(this);
        auto *top = new QHBoxLayout;
        top->addWidget(new QLabel(QStringLiteral("Attendance date:")));
        date_ = new QDateEdit(QDate::currentDate());
        date_->setCalendarPopup(true);
        date_->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
        top->addWidget(date_);
        top->addStretch();
        addButton_ = new QPushButton(QStringLiteral("Add student"));
        removeButton_ = new QPushButton(QStringLiteral("Remove selected"));
        exportButton_ = new QPushButton(QStringLiteral("Export date to CSV"));
        top->addWidget(addButton_);
        top->addWidget(removeButton_);
        top->addWidget(exportButton_);
        root->addLayout(top);

        table_ = new QTableWidget(0, 3);
        table_->setHorizontalHeaderLabels({QStringLiteral("Student ID"), QStringLiteral("Name"), QStringLiteral("Status")});
        table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        table_->setSelectionBehavior(QAbstractItemView::SelectRows);
        table_->setSelectionMode(QAbstractItemView::SingleSelection);
        table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        root->addWidget(table_);

        auto *bottom = new QHBoxLayout;
        status_ = new QComboBox;
        status_->addItems(statuses);
        auto *markButton = new QPushButton(QStringLiteral("Set selected status"));
        summary_ = new QLabel;
        bottom->addWidget(new QLabel(QStringLiteral("Set status:")));
        bottom->addWidget(status_);
        bottom->addWidget(markButton);
        bottom->addStretch();
        bottom->addWidget(summary_);
        root->addLayout(bottom);

        connect(date_, &QDateEdit::dateChanged, this, [this] { reload(); });
        connect(addButton_, &QPushButton::clicked, this, [this] { addStudent(); });
        connect(removeButton_, &QPushButton::clicked, this, [this] { removeStudent(); });
        connect(exportButton_, &QPushButton::clicked, this, [this] { exportCsv(); });
        connect(markButton, &QPushButton::clicked, this, [this] { setSelectedStatus(); });
        reload();
    }

private:
    QDateEdit *date_ = nullptr;
    QTableWidget *table_ = nullptr;
    QComboBox *status_ = nullptr;
    QLabel *summary_ = nullptr;
    QPushButton *addButton_ = nullptr;
    QPushButton *removeButton_ = nullptr;
    QPushButton *exportButton_ = nullptr;

    void reload() {
        table_->setRowCount(0);
        QSqlQuery query;
        query.prepare(QStringLiteral("SELECT s.id,s.name,COALESCE(a.status,'Unmarked') "
                                     "FROM students s LEFT JOIN attendance a "
                                     "ON a.student_id=s.id AND a.date=? ORDER BY s.name COLLATE NOCASE"));
        query.addBindValue(date_->date().toString(Qt::ISODate));
        if (!runQuery(query, QStringLiteral("Could not load attendance."))) return;
        int present = 0, absent = 0, late = 0, excused = 0, unmarked = 0;
        while (query.next()) {
            const int row = table_->rowCount();
            table_->insertRow(row);
            for (int column = 0; column < 3; ++column)
                table_->setItem(row, column, new QTableWidgetItem(query.value(column).toString()));
            const QString state = query.value(2).toString();
            if (state == QStringLiteral("Present")) ++present;
            else if (state == QStringLiteral("Absent")) ++absent;
            else if (state == QStringLiteral("Late")) ++late;
            else if (state == QStringLiteral("Excused")) ++excused;
            else ++unmarked;
        }
        summary_->setText(QStringLiteral("Present %1 | Absent %2 | Late %3 | Excused %4 | Unmarked %5")
                          .arg(present).arg(absent).arg(late).arg(excused).arg(unmarked));
    }

    void addStudent() {
        bool ok = false;
        const QString id = QInputDialog::getText(this, QStringLiteral("Add student"),
                            QStringLiteral("Student ID:"), QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok) return;
        if (id.isEmpty() || id.contains(',') || id.contains('\n')) {
            QMessageBox::warning(this, QStringLiteral("Invalid ID"), QStringLiteral("Enter a non-empty ID without commas or line breaks."));
            return;
        }
        const QString name = QInputDialog::getText(this, QStringLiteral("Add student"),
                              QStringLiteral("Student name:"), QLineEdit::Normal, QString(), &ok).trimmed();
        if (!ok) return;
        if (name.isEmpty() || name.contains(',') || name.contains('\n')) {
            QMessageBox::warning(this, QStringLiteral("Invalid name"), QStringLiteral("Enter a non-empty name without commas or line breaks."));
            return;
        }
        QSqlQuery query;
        query.prepare(QStringLiteral("INSERT INTO students(id,name) VALUES(?,?)"));
        query.addBindValue(id);
        query.addBindValue(name);
        if (!query.exec()) {
            QMessageBox::warning(this, QStringLiteral("Could not add student"),
                                 QStringLiteral("That student ID may already exist.\n\n") + query.lastError().text());
            return;
        }
        reload();
    }

    void removeStudent() {
        const int row = table_->currentRow();
        if (row < 0) {
            QMessageBox::information(this, QStringLiteral("Select a student"), QStringLiteral("Select a student to remove."));
            return;
        }
        const QString id = table_->item(row, 0)->text();
        if (QMessageBox::question(this, QStringLiteral("Remove student"),
            QStringLiteral("Remove student %1 and all their attendance records?").arg(id)) != QMessageBox::Yes) return;
        QSqlQuery query;
        query.prepare(QStringLiteral("DELETE FROM attendance WHERE student_id=?"));
        query.addBindValue(id);
        if (!runQuery(query, QStringLiteral("Could not remove attendance records."))) return;
        query.prepare(QStringLiteral("DELETE FROM students WHERE id=?"));
        query.addBindValue(id);
        if (runQuery(query, QStringLiteral("Could not remove student."))) reload();
    }

    void setSelectedStatus() {
        const int row = table_->currentRow();
        if (row < 0) {
            QMessageBox::information(this, QStringLiteral("Select a student"), QStringLiteral("Select a student first."));
            return;
        }
        QSqlQuery query;
        query.prepare(QStringLiteral("INSERT INTO attendance(student_id,date,status) VALUES(?,?,?) "
                                     "ON CONFLICT(student_id,date) DO UPDATE SET status=excluded.status"));
        query.addBindValue(table_->item(row, 0)->text());
        query.addBindValue(date_->date().toString(Qt::ISODate));
        query.addBindValue(status_->currentText());
        if (runQuery(query, QStringLiteral("Could not save attendance."))) reload();
    }

    void exportCsv() {
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export attendance"),
            QStringLiteral("attendance-%1.csv").arg(date_->date().toString(Qt::ISODate)), QStringLiteral("CSV files (*.csv)"));
        if (path.isEmpty()) return;
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::critical(this, QStringLiteral("Export error"), file.errorString());
            return;
        }
        QTextStream out(&file);
        out << "Date,Student ID,Name,Status\n";
        for (int row = 0; row < table_->rowCount(); ++row)
            out << csvField(date_->date().toString(Qt::ISODate)) << ','
                << csvField(table_->item(row, 0)->text()) << ','
                << csvField(table_->item(row, 1)->text()) << ','
                << csvField(table_->item(row, 2)->text()) << '\n';
        QMessageBox::information(this, QStringLiteral("Export complete"), QStringLiteral("CSV report saved."));
    }
};

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ClassCheck"));
    QApplication::setOrganizationName(QStringLiteral("ClassCheck"));
    if (!openDatabase() || !authenticateOrSetUp()) return 0;
    AttendanceWindow window;
    window.show();
    return app.exec();
}
