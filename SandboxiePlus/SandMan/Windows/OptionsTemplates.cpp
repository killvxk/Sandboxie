#include "stdafx.h"
#include "OptionsWindow.h"
#include "SandMan.h"
#include "SettingsWindow.h"
#include "../MiscHelpers/Common/Settings.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MiscHelpers/Common/ComboInputDialog.h"
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include "Helpers/WinAdmin.h"


void COptionsWindow::LoadTemplates()
{
	m_AllTemplates.clear();
	//ui.cmbCategories->clear();

	QStringList Templates;
	for (int index = 0; ; index++)
	{
		QString Value = m_pBox->GetAPI()->SbieIniGet("", "", index);
		if (Value.isNull())
			break;
		Templates.append(Value);
	}

	for (QStringList::iterator I = Templates.begin(); I != Templates.end();)
	{
		if (I->left(9).compare("Template_", Qt::CaseInsensitive) != 0 || *I == "Template_KnownConflicts") {
			I = Templates.erase(I);
			continue;
		}
	
		QString Name = *I++;
		QString Category = m_pBox->GetAPI()->SbieIniGet(Name, "Tmpl.Class", 0x40000000L); // CONF_GET_NO_GLOBAL);
		QString Title = m_pBox->GetAPI()->SbieIniGet(Name, "Tmpl.Title", 0x40000000L); // CONF_GET_NO_GLOBAL);
		QString Hide = m_pBox->GetAPI()->SbieIniGet(Name, "Tmpl.Hide", 0x40000000L); // CONF_GET_NO_GLOBAL);
		if (Hide == "y" || Hide == "Y")
			continue;
		if (Name == "Template_ScreenReader")
			continue;

		if (Title.left(1) == "#")
		{
			int End = Title.mid(1).indexOf(",");
			if (End == -1) End = Title.length() - 1;
			int MsgNum = Title.mid(1, End).toInt();
			Title = m_pBox->GetAPI()->GetSbieMsgStr(MsgNum, theGUI->m_LanguageId).arg(Title.mid(End + 2)).arg("");
		}
		if (Title.isEmpty()) Title = Name;
		//else Title += " (" + Name + ")";
		if (Title == "-")
			continue; // skip separators

		m_AllTemplates.insertMulti(Category, qMakePair(Name, Title));
	}
	
	if (ui.cmbCategories->count() == 0)
	{
		ui.cmbCategories->addItem(tr("All Categories"), "");
		ui.cmbCategories->setCurrentIndex(0);
		foreach(const QString & Category, m_AllTemplates.uniqueKeys())
		{
			if (Category.isEmpty())
				continue;
			ui.cmbCategories->addItem(GetCategoryName(Category), Category);
		}
	}

	m_GlobalTemplates = m_pBox->GetAPI()->GetGlobalSettings()->GetTextList("Template", false);
	m_BoxTemplates = m_pBox->GetTextList("Template", false);

	LoadFolders();

	ui.chkScreenReaders->setChecked(m_BoxTemplates.contains("ScreenReader"));

	ShowTemplates();

	m_TemplatesChanged = false;
}

void COptionsWindow::SetTemplate(const QString& Template, bool bEnabled)
{
	if(bEnabled)
		m_BoxTemplates.append(Template);
	else
		m_BoxTemplates.removeAll(Template);
	m_TemplatesChanged = true; 
	OnOptChanged();
}

void COptionsWindow::OnScreenReaders()
{ 
	SetTemplate("ScreenReader", ui.chkScreenReaders->isChecked());
}

QString COptionsWindow::GetCategoryName(const QString& Category)
{
	if (Category.compare("Local", Qt::CaseInsensitive) == 0)			return tr("Custom Templates");
	if (Category.compare("EmailReader", Qt::CaseInsensitive) == 0)		return tr("Email Reader");
	if (Category.compare("Print", Qt::CaseInsensitive) == 0)			return tr("PDF/Print");
	if (Category.compare("Security", Qt::CaseInsensitive) == 0)			return tr("Security/Privacy");
	if (Category.compare("Desktop", Qt::CaseInsensitive) == 0)			return tr("Desktop Utilities");
	if (Category.compare("Download", Qt::CaseInsensitive) == 0)			return tr("Download Managers");
	if (Category.compare("Misc", Qt::CaseInsensitive) == 0)				return tr("Miscellaneous");
	if (Category.compare("WebBrowser", Qt::CaseInsensitive) == 0)		return tr("Web Browser");
	if (Category.compare("MediaPlayer", Qt::CaseInsensitive) == 0)		return tr("Media Player");
	if (Category.compare("TorrentClient", Qt::CaseInsensitive) == 0)	return tr("Torrent Client");
	return Category;
}

void COptionsWindow::ShowTemplates()
{
	ui.treeTemplates->clear();

	QString CategoryFilter = ui.cmbCategories->currentData().toString();
	QString TextFilter = ui.txtTemplates->text();

	for (QMultiMap<QString, QPair<QString, QString>>::iterator I = m_AllTemplates.begin(); I != m_AllTemplates.end(); ++I)
	{
		if (!CategoryFilter.isEmpty() && I.key().compare(CategoryFilter, Qt::CaseInsensitive) != 0)
			continue;

		if (I.value().second.indexOf(TextFilter, 0, Qt::CaseInsensitive) == -1)
			continue;

		if (I.key().isEmpty())
			continue; // dont show templates without a category (these are usually deprecated templates)

		QString Name = I.value().first.mid(9);

		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		pItem->setText(0, GetCategoryName(I.key()));
		pItem->setData(1, Qt::UserRole, I.value().first);
		pItem->setText(1, I.value().second);
		//pItem->setFlags(pItem->flags() | Qt::ItemIsUserCheckable);
		if(m_GlobalTemplates.contains(Name))
			pItem->setCheckState(1, Qt::PartiallyChecked);
		else if (m_BoxTemplates.contains(Name))
			pItem->setCheckState(1, Qt::Checked);
		else
			pItem->setCheckState(1, Qt::Unchecked);
		ui.treeTemplates->addTopLevelItem(pItem);
	}

	ShowFolders();
}

void COptionsWindow::OnTemplateClicked(QTreeWidgetItem* pItem, int Column)
{
	QString Name = pItem->data(1, Qt::UserRole).toString().mid(9);
	if (m_GlobalTemplates.contains(Name)) {
		if (pItem->checkState(1) != Qt::PartiallyChecked) {
			QMessageBox::warning(this, "SandboxiePlus", tr("This template is enabled globally. To configure it, use the global options."));
			pItem->setCheckState(1, Qt::PartiallyChecked);
		}
		return;
	}

	if (pItem->checkState(1) == Qt::Checked) {
		if (!m_BoxTemplates.contains(Name)) {
			m_BoxTemplates.append(Name);
			m_TemplatesChanged = true;
			OnOptChanged();
		}
	}
	else if (pItem->checkState(1) == Qt::Unchecked) {
		if (m_BoxTemplates.contains(Name)) {
			m_BoxTemplates.removeAll(Name);
			m_TemplatesChanged = true;
			OnOptChanged();
		}
	}
}

void COptionsWindow::OnTemplateDoubleClicked(QTreeWidgetItem* pItem, int Column)
{
	QSharedPointer<CSbieIni> pTemplate = QSharedPointer<CSbieIni>(new CSbieIni(pItem->data(1, Qt::UserRole).toString(), m_pBox->GetAPI()));

	COptionsWindow OptionsWindow(pTemplate, pItem->text(1));
	OptionsWindow.exec();

	if(pItem->text(0) == "Local")
		LoadTemplates();
}

void COptionsWindow::OnAddTemplates()
{
	QString Value = QInputDialog::getText(this, "Sandboxie-Plus", tr("Please enter the template identifier"), QLineEdit::Normal);
	if (Value.isEmpty())
		return;

	QString Name = QString(Value).replace(" ", "_");

	SB_STATUS Status = m_pBox->GetAPI()->ValidateName(Name);
	if (Status.IsError()) {
		QMessageBox::critical(this, "Sandboxie-Plus", tr("Error: %1").arg(CSandMan::FormatError(Status)));
		return;
	}

	QSharedPointer<CSbieIni> pTemplate = QSharedPointer<CSbieIni>(new CSbieIni("Template_Local_" + Name, m_pBox->GetAPI()));

	pTemplate->SetText("Tmpl.Title", Value);
	pTemplate->SetText("Tmpl.Class", "Local");

	COptionsWindow OptionsWindow(pTemplate, Value);
	OptionsWindow.exec();

	LoadTemplates();
}

void COptionsWindow::OnDelTemplates()
{
	QTreeWidgetItem* pItem = ui.treeTemplates->currentItem();
	if (!pItem || pItem->text(0) != "Local")
	{
		QMessageBox::critical(this, "Sandboxie-Plus", tr("Only local templates can be removed!"));
		return;
	}

	if (QMessageBox("Sandboxie-Plus", tr("Do you really want to delete the selected local template?"), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton, this).exec() != QMessageBox::Yes)
		return;

	// delete section
	m_pBox->GetAPI()->SbieIniSet(pItem->data(1, Qt::UserRole).toString(), "*", "");

	LoadTemplates();
}

void COptionsWindow::SaveTemplates()
{
	WriteTextList("Template", m_BoxTemplates);

	m_TemplatesChanged = false;
}

void COptionsWindow::LoadFolders()
{
	m_BoxFolders.clear();
	foreach(const QString &Name, m_BoxTemplates)
	{
		QSharedPointer<CSbieIni> pTemplate = QSharedPointer<CSbieIni>(new CSbieIni("Template_" + Name, m_pBox->GetAPI()));

		QList<QPair<QString, QString>> AllValues = pTemplate->GetIniSection(NULL, true);
		for (QList<QPair<QString, QString>>::const_iterator I = AllValues.begin(); I != AllValues.end(); ++I)
		{
			QString Value = I->second;

			int begin = Value.indexOf("%Tmpl.");
			if (begin == -1) continue;
			int end = Value.indexOf("%", begin + 6);
			if (end == -1) continue;
			QString Tmpl = Value.mid(begin + 1, end - begin - 1);
			if (!m_BoxFolders.contains(Tmpl))
				m_BoxFolders.append(Tmpl);				
		}
	}

	m_FoldersChanged = false;
}

void COptionsWindow::OnFolderChanged()
{
	//CPathEdit* pEdit = (CPathEdit*)sender();
	m_FoldersChanged = true;
	OnOptChanged();
}

void COptionsWindow::ShowFolders()
{
	QSharedPointer<CSbieIni> pTemplateSettings = QSharedPointer<CSbieIni>(new CSbieIni("TemplateSettings", m_pBox->GetAPI()));

	QString UserName = m_pBox->GetAPI()->GetCurrentUserName();

	ui.treeFolders->clear();
	foreach(const QString &Folder, m_BoxFolders) 
	{
		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		pItem->setText(0, Folder);
		ui.treeFolders->addTopLevelItem(pItem);

		CPathEdit* pEdit = new CPathEdit(true);
		pEdit->SetWindowsPaths();
		pEdit->SetDefault(pTemplateSettings->GetText(Folder));
		pEdit->SetText(pTemplateSettings->GetText(Folder + "." + UserName));
		connect(pEdit, SIGNAL(textChanged(const QString&)), this, SLOT(OnFolderChanged()));
		ui.treeFolders->setItemWidget(pItem, 1, pEdit);
	}
}

void COptionsWindow::SaveFolders()
{
	QSharedPointer<CSbieIni> pTemplateSettings = QSharedPointer<CSbieIni>(new CSbieIni("TemplateSettings", m_pBox->GetAPI()));

	QString UserName = m_pBox->GetAPI()->GetCurrentUserName();

	for (int i = 0; i < ui.treeFolders->topLevelItemCount(); i++)
	{
		QTreeWidgetItem* pItem = ui.treeFolders->topLevelItem(i);

		QString Folder = pItem->text(0);
		CPathEdit* pEdit = (CPathEdit*)ui.treeFolders->itemWidget(pItem, 1);

		QString Path = pEdit->GetText();
		if (Path.isEmpty())
			pTemplateSettings->DelValue(Folder + "." + UserName);
		else
			pTemplateSettings->SetText(Folder + "." + UserName, Path);
	}

	m_FoldersChanged = false;
}
