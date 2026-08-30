/****************************************************************************
** Meta object code from reading C++ file 'PianoRollEditor.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../include/Editor/PianoRollEditor.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'PianoRollEditor.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN7myvocal15PianoRollEditorE_t {};
} // unnamed namespace

template <> constexpr inline auto myvocal::PianoRollEditor::qt_create_metaobjectdata<qt_meta_tag_ZN7myvocal15PianoRollEditorE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "myvocal::PianoRollEditor",
        "documentChanged",
        "",
        "noteDoubleClicked",
        "id",
        "requestPlaybackTick",
        "tick"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'documentChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'noteDoubleClicked'
        QtMocHelpers::SignalData<void(qint64)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 4 },
        }}),
        // Signal 'requestPlaybackTick'
        QtMocHelpers::SignalData<void(qint64)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 6 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<PianoRollEditor, qt_meta_tag_ZN7myvocal15PianoRollEditorE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject myvocal::PianoRollEditor::staticMetaObject = { {
    QMetaObject::SuperData::link<QAbstractScrollArea::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7myvocal15PianoRollEditorE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7myvocal15PianoRollEditorE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN7myvocal15PianoRollEditorE_t>.metaTypes,
    nullptr
} };

void myvocal::PianoRollEditor::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<PianoRollEditor *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->documentChanged(); break;
        case 1: _t->noteDoubleClicked((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1]))); break;
        case 2: _t->requestPlaybackTick((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (PianoRollEditor::*)()>(_a, &PianoRollEditor::documentChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (PianoRollEditor::*)(qint64 )>(_a, &PianoRollEditor::noteDoubleClicked, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (PianoRollEditor::*)(qint64 )>(_a, &PianoRollEditor::requestPlaybackTick, 2))
            return;
    }
}

const QMetaObject *myvocal::PianoRollEditor::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *myvocal::PianoRollEditor::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7myvocal15PianoRollEditorE_t>.strings))
        return static_cast<void*>(this);
    return QAbstractScrollArea::qt_metacast(_clname);
}

int myvocal::PianoRollEditor::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QAbstractScrollArea::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void myvocal::PianoRollEditor::documentChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void myvocal::PianoRollEditor::noteDoubleClicked(qint64 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void myvocal::PianoRollEditor::requestPlaybackTick(qint64 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}
QT_WARNING_POP
