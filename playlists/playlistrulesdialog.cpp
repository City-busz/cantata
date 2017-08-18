/*
 * Cantata
 *
 * Copyright (c) 2011-2017 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "playlistrulesdialog.h"
#include "playlistruledialog.h"
#include "support/messagebox.h"
#include "widgets/basicitemdelegate.h"
#include <QIcon>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QSpinBox>

class RulesSort : public QSortFilterProxyModel
{
public:
    RulesSort(QObject *parent)
        : QSortFilterProxyModel(parent) {
        setDynamicSortFilter(true);
        setFilterCaseSensitivity(Qt::CaseInsensitive);
        setSortCaseSensitivity(Qt::CaseInsensitive);
        setSortLocaleAware(true);
        sort(0);
    }

    virtual ~RulesSort() {
    }

    bool lessThan(const QModelIndex &left, const QModelIndex &right) const {
        bool lInc=left.data(Qt::UserRole+2).toBool();
        bool rInc=right.data(Qt::UserRole+2).toBool();

        if (lInc==rInc) {
            return left.data(Qt::DisplayRole).toString().localeAwareCompare(right.data(Qt::DisplayRole).toString())<0;
        } else {
            return lInc ? true : false;
        }
    }
};

static QString translateStr(const QString &key)
{
    if (DynamicPlaylists::constArtistKey==key) {
        return QObject::tr("Artist");
    } else if (DynamicPlaylists::constSimilarArtistsKey==key) {
        return QObject::tr("SimilarArtists");
    } else if (DynamicPlaylists::constAlbumArtistKey==key) {
        return QObject::tr("AlbumArtist");
    } else if (DynamicPlaylists::constComposerKey==key) {
        return QObject::tr("Composer");
    } else if (DynamicPlaylists::constCommentKey==key) {
        return QObject::tr("Comment");
    } else if (DynamicPlaylists::constAlbumKey==key) {
        return QObject::tr("Album");
    } else if (DynamicPlaylists::constTitleKey==key) {
        return QObject::tr("Title");
    } else if (DynamicPlaylists::constGenreKey==key) {
        return QObject::tr("Genre");
    } else if (DynamicPlaylists::constDateKey==key) {
        return QObject::tr("Date");
    } else if (DynamicPlaylists::constFileKey==key) {
        return QObject::tr("File");
    } else {
        return key;
    }
}

static void update(QStandardItem *i, const DynamicPlaylists::Rule &rule)
{
    DynamicPlaylists::Rule::ConstIterator it(rule.constBegin());
    DynamicPlaylists::Rule::ConstIterator end(rule.constEnd());
    QMap<QString, QVariant> v;
    QString str;
    QString type=QObject::tr("Include");
    bool exact=true;
    bool include=true;

    for (int count=0; it!=end; ++it, ++count) {
        if (DynamicPlaylists::constExcludeKey==it.key()) {
            if (QLatin1String("true")==it.value()) {
                type=QObject::tr("Exclude");
                include=false;
            }
        } else if (DynamicPlaylists::constExactKey==it.key()) {
            if (QLatin1String("false")==it.value()) {
                exact=false;
            }
        } else {
            str+=QString("%1=%2").arg(translateStr(it.key()), it.value());
            if (count<rule.count()-1) {
                str+=' ';
            }
        }
        v.insert(it.key(), it.value());
    }

    if (str.isEmpty()) {
        str=type;
    } else {
        str=type+' '+str;
    }

    if (exact) {
        str+=QObject::tr(" (Exact)");
    }
    i->setText(str);
    i->setData(v);
    i->setData(include, Qt::UserRole+2);
    i->setFlags(Qt::ItemIsSelectable| Qt::ItemIsEnabled);
}

PlaylistRulesDialog::PlaylistRulesDialog(QWidget *parent)
    : Dialog(parent, "PlaylistRulesDialog")
    , dlg(0)
{
    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    setMainWidget(mainWidet);
    setButtons(Ok|Cancel);
    enableButton(Ok, false);
    setCaption(tr("Dynamic Rules"));
    setAttribute(Qt::WA_DeleteOnClose);
    connect(addBtn, SIGNAL(clicked()), SLOT(add()));
    connect(editBtn, SIGNAL(clicked()), SLOT(edit()));
    connect(removeBtn, SIGNAL(clicked()), SLOT(remove()));
    connect(rulesList, SIGNAL(itemsSelected(bool)), SLOT(controlButtons()));
    connect(nameText, SIGNAL(textChanged(const QString &)), SLOT(enableOkButton()));
    connect(aboutLabel, SIGNAL(leftClickedUrl()), this, SLOT(showAbout()));
    connect(DynamicPlaylists::self(), SIGNAL(saved(bool)), SLOT(saved(bool)));

    messageWidget->setVisible(false);
    model=new QStandardItemModel(this);
    proxy=new RulesSort(this);
    proxy->setSourceModel(model);
    rulesList->setModel(proxy);
    rulesList->setItemDelegate(new BasicItemDelegate(rulesList));
    rulesList->setAlternatingRowColors(false);

    minDuration->setSpecialValueText(tr("No Limit"));
    maxDuration->setSpecialValueText(tr("No Limit"));

    controlButtons();
    resize(500, 240);

    static bool registered=false;
    if (!registered) {
        qRegisterMetaType<DynamicPlaylists::Rule>("Dynamic::Rule");
        registered=true;
    }
}

PlaylistRulesDialog::~PlaylistRulesDialog()
{
}

void PlaylistRulesDialog::edit(const QString &name)
{
    DynamicPlaylists::Entry e=DynamicPlaylists::self()->entry(name);
    if (model->rowCount()) {
        model->removeRows(0, model->rowCount());
    }
    nameText->setText(name);
    foreach (const DynamicPlaylists::Rule &r, e.rules) {
        QStandardItem *item = new QStandardItem();
        ::update(item, r);
        model->setItem(model->rowCount(), 0, item);
    }
    origName=name;
    ratingFrom->setValue(e.ratingFrom);
    ratingTo->setValue(e.ratingTo);
    minDuration->setValue(e.minDuration);
    maxDuration->setValue(e.maxDuration);
    numTracks->setValue(e.numTracks);
    show();
}

void PlaylistRulesDialog::enableOkButton()
{
    bool enable=!nameText->text().trimmed().isEmpty();
    enableButton(Ok, enable);
}

void PlaylistRulesDialog::slotButtonClicked(int button)
{
    switch (button) {
    case Ok: {
        if (save()) {
            accept();
        }
        break;
    }
    case Cancel:
        reject();
        // Need to call this - if not, when dialog is closed by window X control, it is not deleted!!!!
        Dialog::slotButtonClicked(button);
        break;
    default:
        break;
    }
}

void PlaylistRulesDialog::controlButtons()
{
    int numSel=rulesList->selectedIndexes().count();
    removeBtn->setEnabled(numSel>0);
    editBtn->setEnabled(1==numSel);
}

void PlaylistRulesDialog::add()
{
    if (!dlg) {
        dlg=new PlaylistRuleDialog(this);
        connect(dlg, SIGNAL(addRule(const DynamicPlaylists::Rule&)), SLOT(addRule(const DynamicPlaylists::Rule&)));
    }
    dlg->createNew();
}

void PlaylistRulesDialog::addRule(const DynamicPlaylists::Rule &rule)
{
    QStandardItem *item = new QStandardItem();
    ::update(item, rule);
    int index=indexOf(item);
    if (-1!=index) {
        delete item;
    } else {
        model->setItem(model->rowCount(), 0, item);
    }
}

void PlaylistRulesDialog::edit()
{
    QModelIndexList items=rulesList->selectedIndexes();

    if (1!=items.count()) {
        return;
    }
    if (!dlg) {
        dlg=new PlaylistRuleDialog(this);
        connect(dlg, SIGNAL(addRule(const DynamicPlaylists::Rule&)), SLOT(addRule(const DynamicPlaylists::Rule&)));
    }
    QModelIndex index=proxy->mapToSource(items.at(0));
    QStandardItem *item=model->itemFromIndex(index);
    DynamicPlaylists::Rule rule;
    QMap<QString, QVariant> v=item->data().toMap();
    QMap<QString, QVariant>::ConstIterator it(v.constBegin());
    QMap<QString, QVariant>::ConstIterator end(v.constEnd());
    for (; it!=end; ++it) {
        rule.insert(it.key(), it.value().toString());
    }
    if (dlg->edit(rule)) {
        ::update(item, dlg->rule());
        int idx=indexOf(item, true);
        if (-1!=idx && idx!=index.row()) {
            model->removeRow(index.row());
        }
    }
}

void PlaylistRulesDialog::remove()
{
    QModelIndexList items=rulesList->selectedIndexes();
    QList<int> rows;
    foreach (const QModelIndex &i, items) {
        rows.append(proxy->mapToSource(i).row());
    }
    qSort(rows);
    while (rows.count()) {
        model->removeRow(rows.takeLast());
    }
}

void PlaylistRulesDialog::showAbout()
{
    MessageBox::information(this,
                         #ifdef Q_OS_MAC
                         tr("About dynamic rules")+QLatin1String("<br/><br/>")+
                         #endif
                         tr("<p>Cantata will query your library using all of the rules listed. "
                              "The list of <i>Include</i> rules will be used to build a set of songs that can be used. "
                              "The list of <i>Exclude</i> rules will be used to build a set of songs that cannot be used. "
                              "If there are no <i>Include</i> rules, Cantata will assume that all songs (bar those from <i>Exclude</i>) can be used.</p>"
                              "<p>e.g. to have Cantata look for 'Rock songs by Wibble OR songs by Various Artists', you would need the following: "
                              "<ul><li>Include AlbumArtist=Wibble Genre=Rock</li><li>Include AlbumArtist=Various Artists</li></ul> "
                              "To have Cantata look for 'Songs by Wibble but not from album Abc', you would need the following: "
                              "<ul><li>Include AlbumArtist=Wibble</li><li>Exclude AlbumArtist=Wibble Album=Abc</li></ul>"
                              "After the set of usable songs has been created, Cantata will randomly select songs to "
                              "keep the play queue filled with specified number of entries (10 by default). If a range of ratings has been specified, then "
                              "only songs with a rating within this range will be used. Likewise, if a duration has been set.</p>")
                        );

}

void PlaylistRulesDialog::saved(bool s)
{
    if (s) {
        accept();
    } else {
        messageWidget->setError(tr("Failed to save %1").arg(nameText->text().trimmed()));
        controls->setEnabled(true);
    }
}

bool PlaylistRulesDialog::save()
{
    if (!controls->isEnabled()) {
        return false;
    }

    QString name=nameText->text().trimmed();

    if (name.isEmpty()) {
        return false;
    }

    if (name!=origName && DynamicPlaylists::self()->exists(name) &&
        MessageBox::No==MessageBox::warningYesNo(this, tr("A set of rules named '%1' already exists!\n\nOverwrite?").arg(name),
                                                  tr("Overwrite Rules"), StdGuiItem::overwrite(), StdGuiItem::cancel())) {
        return false;
    }

    DynamicPlaylists::Entry entry;
    entry.name=name;
    int from=ratingFrom->value();
    int to=ratingTo->value();
    entry.ratingFrom=qMin(from, to);
    entry.ratingTo=qMax(from, to);
    from=minDuration->value();
    to=maxDuration->value();
    if (to>0) {
        entry.minDuration=qMin(from, to);
        entry.maxDuration=qMax(from, to);
    } else {
        entry.minDuration=from;
        entry.maxDuration=to;
    }
    entry.numTracks=numTracks->value();

    for (int i=0; i<model->rowCount(); ++i) {
        QStandardItem *itm=model->item(i);
        if (itm) {
            QMap<QString, QVariant> v=itm->data().toMap();
            QMap<QString, QVariant>::ConstIterator it(v.constBegin());
            QMap<QString, QVariant>::ConstIterator end(v.constEnd());
            DynamicPlaylists::Rule rule;
            for (; it!=end; ++it) {
                rule.insert(it.key(), it.value().toString());
            }
            entry.rules.append(rule);
        }
    }

    bool saved=DynamicPlaylists::self()->save(entry);

    if (DynamicPlaylists::self()->isRemote()) {
        if (saved) {
            messageWidget->setInformation(tr("Saving %1").arg(name));
            controls->setEnabled(false);
            enableButton(Ok, false);
        }
        return false;
    } else {
        if (saved && !origName.isEmpty() && entry.name!=origName) {
            DynamicPlaylists::self()->del(origName);
        }
        return saved;
    }
}

int PlaylistRulesDialog::indexOf(QStandardItem *item, bool diff)
{
    QMap<QString, QVariant> v=item->data().toMap();

    for (int i=0; i<model->rowCount(); ++i) {
        QStandardItem *itm=model->item(i);
        if (itm) {
            if (itm->data().toMap()==v && (!diff || itm!=item)) {
                return i;
            }
        }
    }
    return -1;
}