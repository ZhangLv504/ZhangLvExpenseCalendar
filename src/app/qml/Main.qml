import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 420
    height: 820
    minimumWidth: 320
    minimumHeight: 560
    visible: true
    title: qsTr("掌律记账")

    StackView {
        id: stack
        anchors.fill: parent
        initialItem: listPage
    }

    Component {
        id: listPage
        Page {
            header: ToolBar {
                RowLayout {
                    anchors.fill: parent
                    ToolButton {
                        text: qsTr("‹")
                        enabled: app.currentMonth > 1 || app.currentYear > 2000
                        onClicked: {
                            var y = app.currentYear;
                            var m = app.currentMonth - 1;
                            if (m < 1) { m = 12; y -= 1; }
                            app.currentYear = y;
                            app.currentMonth = m;
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Qt.AlignHCenter
                        text: app.currentYear + qsTr("年") + app.currentMonth + qsTr("月")
                        font.pixelSize: 18
                        font.bold: true
                    }
                    ToolButton {
                        text: qsTr("›")
                        onClicked: {
                            var y = app.currentYear;
                            var m = app.currentMonth + 1;
                            if (m > 12) { m = 1; y += 1; }
                            app.currentYear = y;
                            app.currentMonth = m;
                        }
                    }
                    ToolButton {
                        text: qsTr("分类")
                        onClicked: stack.push(categoryPage)
                    }
                }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    color: "#F5F5F5"
                    radius: 8
                    height: 76
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { text: qsTr("收入"); color: "#666"; font.pixelSize: 13 }
                            Label { text: app.monthIncomeText; color: "#C62828"; font.pixelSize: 20; font.bold: true }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { text: qsTr("支出"); color: "#666"; font.pixelSize: 13 }
                            Label { text: app.monthExpenseText; color: "#2E7D32"; font.pixelSize: 20; font.bold: true }
                        }
                    }
                }

                Label {
                    text: qsTr("明细")
                    font.pixelSize: 14
                    color: "#888"
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: app.model
                    delegate: ItemDelegate {
                        width: ListView.view.width
                        height: 62
                        onClicked: stack.push(editPage, { isNew: false, row: model.index })
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            ColumnLayout {
                                Layout.fillWidth: true
                                Label { text: model.category; font.pixelSize: 15 }
                                Label {
                                    text: (model.note ? model.note + "  " : "") + model.timeText
                                    font.pixelSize: 11
                                    color: "#999"
                                }
                            }
                            Label {
                                text: (model.txType === 1 ? "+" : "-") + model.amountText
                                color: model.txType === 1 ? "#C62828" : "#2E7D32"
                                font.pixelSize: 16
                                font.bold: true
                            }
                        }
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: app.model.count === 0
                        text: qsTr("本月还没有账目")
                        color: "#AAA"
                    }
                }
            }

            Button {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 20
                width: 116
                height: 48
                text: qsTr("+ 记一笔")
                highlighted: true
                onClicked: stack.push(editPage, { isNew: true, row: -1 })
            }
        }
    }

    // 记一笔 / 编辑页
    Component {
        id: editPage
        Page {
            property bool isNew: true
            property int row: -1
            property int selectedType: 0

            function applyType(t) {
                selectedType = t;
                incomeRadio.checked = (t === 1);
                expenseRadio.checked = (t === 0);
                categoryCombo.model = (t === 1) ? app.incomeCategories() : app.expenseCategories();
                categoryCombo.currentIndex = 0;
            }

            Component.onCompleted: {
                if (!isNew) {
                    var rec = app.recordAt(row);
                    amountField.text = rec.amountText;
                    noteField.text = rec.note;
                    dateField.text = rec.dateText;
                    applyType(rec.type);
                    var cats = (rec.type === 1) ? app.incomeCategories() : app.expenseCategories();
                    categoryCombo.currentIndex = cats.indexOf(rec.category);
                } else {
                    dateField.text = Qt.formatDateTime(new Date(), "yyyy-MM-dd HH:mm");
                    applyType(0);
                }
                errorLabel.text = "";
            }

            header: ToolBar {
                RowLayout {
                    anchors.fill: parent
                    ToolButton {
                        text: qsTr("‹ 返回")
                        onClicked: stack.pop()
                    }
                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Qt.AlignHCenter
                        text: isNew ? qsTr("记一笔") : qsTr("编辑账目")
                        font.pixelSize: 18
                        font.bold: true
                    }
                }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    ButtonGroup { id: typeGroup }
                    RadioButton {
                        id: expenseRadio
                        text: qsTr("支出")
                        ButtonGroup.group: typeGroup
                        onCheckedChanged: if (checked) applyType(0)
                    }
                    RadioButton {
                        id: incomeRadio
                        text: qsTr("收入")
                        ButtonGroup.group: typeGroup
                        onCheckedChanged: if (checked) applyType(1)
                    }
                }

                Label { text: qsTr("分类"); color: "#888"; font.pixelSize: 13 }
                ComboBox {
                    id: categoryCombo
                    Layout.fillWidth: true
                    model: app.expenseCategories()
                }

                Label { text: qsTr("金额"); color: "#888"; font.pixelSize: 13 }
                TextField {
                    id: amountField
                    Layout.fillWidth: true
                    placeholderText: qsTr("如 12.34")
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                }

                Label { text: qsTr("日期时间"); color: "#888"; font.pixelSize: 13 }
                TextField {
                    id: dateField
                    Layout.fillWidth: true
                    placeholderText: "yyyy-MM-dd HH:mm"
                }

                Label { text: qsTr("备注"); color: "#888"; font.pixelSize: 13 }
                TextField {
                    id: noteField
                    Layout.fillWidth: true
                    placeholderText: qsTr("可选")
                }

                Label {
                    id: errorLabel
                    color: "#C62828"
                    font.pixelSize: 12
                    visible: text.length > 0
                }

                Item { Layout.fillHeight: true }

                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        visible: !isNew
                        text: qsTr("删除")
                        onClicked: stack.push(confirmDeletePage, { row: row })
                    }
                    Button {
                        Layout.fillWidth: true
                        text: qsTr("保存")
                        highlighted: true
                        onClicked: {
                            var cat = categoryCombo.currentText;
                            var msg = isNew
                                ? app.addRecord(selectedType, amountField.text, cat, noteField.text, dateField.text)
                                : app.updateRecord(row, selectedType, amountField.text, cat, noteField.text, dateField.text);
                            if (msg) errorLabel.text = msg;
                            else stack.pop();
                        }
                    }
                }
            }
        }
    }

    // 删除确认页（普通 Page，避开 Dialog 渲染问题）
    Component {
        id: confirmDeletePage
        Page {
            property int row: -1
            header: ToolBar {
                Label {
                    anchors.centerIn: parent
                    text: qsTr("确认删除")
                    font.pixelSize: 18
                    font.bold: true
                }
            }
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 20
                Label {
                    text: qsTr("确定删除这条账目？此操作不可撤销。")
                    font.pixelSize: 15
                }
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Button {
                        text: qsTr("取消")
                        onClicked: stack.pop()
                    }
                    Button {
                        text: qsTr("确认删除")
                        onClicked: {
                            var msg = app.deleteRecord(row);
                            if (msg) {
                                errorText.text = msg;
                            } else {
                                // 回退到明细页（编辑页 + 确认页两层）
                                stack.pop();
                                stack.pop();
                            }
                        }
                    }
                }
                Label {
                    id: errorText
                    color: "#C62828"
                    font.pixelSize: 12
                    visible: text.length > 0
                }
            }
        }
    }

    // 分类管理页
    Component {
        id: categoryPage
        Page {
            property int editingType: 0

            header: ToolBar {
                RowLayout {
                    anchors.fill: parent
                    ToolButton {
                        text: qsTr("‹ 返回")
                        onClicked: stack.pop()
                    }
                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Qt.AlignHCenter
                        text: qsTr("分类管理")
                        font.pixelSize: 18
                        font.bold: true
                    }
                }
            }

            ColumnLayout {
                id: catLayout
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                function reloadCategories() {
                    var details = app.categoryDetails(editingType);
                    catModel.clear();
                    for (var i = 0; i < details.length; ++i) {
                        catModel.append({ name: details[i].name, builtin: details[i].builtin });
                    }
                    catError.text = "";
                }

                function addNewCategory() {
                    var msg = app.addCategory(editingType, newCatField.text);
                    if (msg) catError.text = msg;
                    else newCatField.text = "";
                }

                Component.onCompleted: catLayout.reloadCategories()
                Connections {
                    target: app
                    function onCategoriesChanged() { catLayout.reloadCategories() }
                }

                RowLayout {
                    Layout.fillWidth: true
                    ButtonGroup { id: catTypeGroup }
                    RadioButton {
                        id: catExpense
                        text: qsTr("支出分类")
                        checked: true
                        ButtonGroup.group: catTypeGroup
                        onCheckedChanged: if (checked) { editingType = 0; catLayout.reloadCategories() }
                    }
                    RadioButton {
                        text: qsTr("收入分类")
                        ButtonGroup.group: catTypeGroup
                        onCheckedChanged: if (checked) { editingType = 1; catLayout.reloadCategories() }
                    }
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: ListModel { id: catModel }
                    delegate: ItemDelegate {
                        width: ListView.view.width
                        height: 50
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            Label {
                                Layout.fillWidth: true
                                text: model.name
                                font.pixelSize: 15
                            }
                            Label {
                                text: model.builtin ? qsTr("内置") : ""
                                color: "#999"
                                font.pixelSize: 11
                            }
                            Button {
                                text: qsTr("删除")
                                enabled: !model.builtin
                                onClicked: app.removeCategory(editingType, model.name)
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    TextField {
                        id: newCatField
                        Layout.fillWidth: true
                        placeholderText: qsTr("新分类名称")
                        onAccepted: catLayout.addNewCategory()
                    }
                    Button {
                        text: qsTr("添加")
                        onClicked: catLayout.addNewCategory()
                    }
                }

                Label {
                    id: catError
                    color: "#C62828"
                    font.pixelSize: 12
                    visible: text.length > 0
                }
            }
        }
    }
}
