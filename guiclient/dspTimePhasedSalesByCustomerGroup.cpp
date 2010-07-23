/*
 * This file is part of the xTuple ERP: PostBooks Edition, a free and
 * open source Enterprise Resource Planning software suite,
 * Copyright (c) 1999-2010 by OpenMFG LLC, d/b/a xTuple.
 * It is licensed to you under the Common Public Attribution License
 * version 1.0, the full text of which (including xTuple-specific Exhibits)
 * is available at www.xtuple.com/CPAL.  By using this software, you agree
 * to be bound by its terms.
 */

#include "dspTimePhasedSalesByCustomerGroup.h"

#include <QAction>
#include <QMenu>
#include <QMessageBox>
#include <QVariant>

#include <parameter.h>
#include <dbtools.h>
#include <datecluster.h>
#include <openreports.h>
#include <metasql.h>

#include "dspSalesHistoryByCustomer.h"
#include "guiclient.h"
#include "submitReport.h"
#include "mqlutil.h"

dspTimePhasedSalesByCustomerGroup::dspTimePhasedSalesByCustomerGroup(QWidget* parent, const char* name, Qt::WFlags fl)
    : XWidget(parent, name, fl)
{
  setupUi(this);

  connect(_print, SIGNAL(clicked()), this, SLOT(sPrint()));
  connect(_sohist, SIGNAL(populateMenu(QMenu*,QTreeWidgetItem*,int)), this, SLOT(sPopulateMenu(QMenu*,QTreeWidgetItem*,int)));
  connect(_close, SIGNAL(clicked()), this, SLOT(close()));
  connect(_query, SIGNAL(clicked()), this, SLOT(sFillList()));
  connect(_calendar, SIGNAL(newCalendarId(int)), _periods, SLOT(populate(int)));
  connect(_calendar, SIGNAL(select(ParameterList&)), _periods, SLOT(load(ParameterList&)));
  connect(_submit, SIGNAL(clicked()), this, SLOT(sSubmit()));
  
  if (!_metrics->boolean("EnableBatchManager"))
    _submit->hide();
    
  _customerGroup->setType(ParameterGroup::CustomerGroup);
  _productCategory->setType(ParameterGroup::ProductCategory);
  
  _sohist->addColumn(tr("Cust. #"),  _orderColumn, Qt::AlignLeft,   true,  "cust_number" );
  _sohist->addColumn(tr("Customer"), 180,          Qt::AlignLeft,   true,  "cust_name" );
}

dspTimePhasedSalesByCustomerGroup::~dspTimePhasedSalesByCustomerGroup()
{
  // no need to delete child widgets, Qt does it all for us
}

void dspTimePhasedSalesByCustomerGroup::languageChange()
{
  retranslateUi(this);
}

void dspTimePhasedSalesByCustomerGroup::sPrint()
{
  ParameterList params(buildParameters());

  orReport report("TimePhasedSalesHistoryByCustomerGroup", params);
  if (report.isValid())
    report.print();
  else
    report.reportError(this);
}

void dspTimePhasedSalesByCustomerGroup::sViewShipments()
{
  ParameterList params;
  params.append("cust_id", _sohist->id());
  params.append("startDate", _columnDates[_column - 2].startDate);
  params.append("endDate", _columnDates[_column - 2].endDate);
  params.append("run");

  if (_productCategory->isSelected())
    params.append("prodcat_id", _productCategory->id());
  else if (_productCategory->isPattern())
    params.append("prodcat_pattern", _productCategory->pattern());

  dspSalesHistoryByCustomer *newdlg = new dspSalesHistoryByCustomer();
  newdlg->set(params);
  omfgThis->handleNewWindow(newdlg);
}

void dspTimePhasedSalesByCustomerGroup::sPopulateMenu(QMenu *menuThis, QTreeWidgetItem *, int pColumn)
{
  QAction *menuItem;

  _column = pColumn;

  if (pColumn > 1)
  {
    menuItem = menuThis->addAction(tr("View Sales Detail..."), this, SLOT(sViewShipments()));
    menuItem->setEnabled(_privileges->check("ViewSalesHistory"));
  }
}

void dspTimePhasedSalesByCustomerGroup::sFillList()
{
  _sohist->clear();

  if (!_periods->isPeriodSelected())
    return;

  _sohist->clear();
  _sohist->setColumnCount(2);

  ParameterList params;
  if (! setParams(params))
    return;

  QList<XTreeWidgetItem*> selected = _periods->selectedItems();
  for (int i = 0; i < selected.size(); i++)
  {
    PeriodListViewItem *cursor = (PeriodListViewItem*)selected[i];
    QString bucketname = QString("bucket_%1").arg(cursor->id());
    _sohist->addColumn(formatDate(cursor->startDate()), _qtyColumn, Qt::AlignRight, true, bucketname);
    _columnDates.append(DatePair(cursor->startDate(), cursor->endDate()));
  }

  MetaSQLQuery mql = mqlLoad("timePhasedSalesByCustomer", "detail");
  q = mql.toQuery(params);

  _sohist->populate(q);
}

bool dspTimePhasedSalesByCustomerGroup::setParams(ParameterList & params)
{
  params.append("period_list",_periods->periodList());

  if (_productCategory->isSelected())
    params.append("prodcat_id", _productCategory->id());
  else if(_productCategory->isPattern())
  {
    QString pattern = _productCategory->pattern();
    if (pattern.length() == 0)
      return false;
    params.append("prodcat_pattern", _productCategory->pattern());
  }

  if (_customerGroup->isSelected())
    params.append("custgrp_id", _customerGroup->id());
  else if (_customerGroup->isPattern())
  {
    QString pattern = _customerGroup->pattern();
    if (pattern.length() == 0)
      return false;
    params.append("custgrp_pattern", _customerGroup->pattern());
  }
  params.append("byCustomerGroup");

  return true;
}

void dspTimePhasedSalesByCustomerGroup::sSubmit()
{
  if (_periods->isPeriodSelected())
  {
    ParameterList params(buildParameters());
    params.append("report_name", "TimePhasedSalesHistoryByCustomerGroup");
    
    submitReport newdlg(this, "", TRUE);
    newdlg.set(params);

    if (newdlg.check() == cNoReportDefinition)
      QMessageBox::critical( this, tr("Report Definition Not Found"),
                             tr( "The report defintions for this report, \"TimePhasedSalesHistoryByCustomerGroup\" cannot be found.\n"
                                 "Please contact your Systems Administrator and report this issue." ) );
    else
      newdlg.exec();
  }
  else
    QMessageBox::critical( this, tr("Incomplete criteria"),
                           tr( "The criteria you specified is not complete. Please make sure all\n"
                               "fields are correctly filled out before running the report." ) );
}

ParameterList dspTimePhasedSalesByCustomerGroup::buildParameters()
{
  ParameterList params;

  _customerGroup->appendValue(params);
  _productCategory->appendValue(params);

  QList<XTreeWidgetItem*> selected = _periods->selectedItems();
  QList<QVariant> periodList;
  for (int i = 0; i < selected.size(); i++)
    periodList.append(((XTreeWidgetItem*)selected[i])->id());
  params.append("period_id_list", periodList);

  params.append("orderByCustomer");

  return params;
}
