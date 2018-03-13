/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   MMSEngineDBFacade.cpp
 * Author: giuliano
 * 
 * Created on January 27, 2018, 9:38 AM
 */

#include <random>
#include "catralibraries/Encrypt.h"
#include "MMSEngineDBFacade.h"

// http://download.nust.na/pub6/mysql/tech-resources/articles/mysql-connector-cpp.html#trx

MMSEngineDBFacade::MMSEngineDBFacade(
        Json::Value configuration,
        shared_ptr<spdlog::logger> logger) 
{
    _logger     = logger;
    
    _defaultContentProviderName     = "default";
    _defaultTerritoryName           = "default";
    
    size_t dbPoolSize = configuration["database"].get("poolSize", 5).asInt();
    string dbServer = configuration["database"].get("server", "XXX").asString();
    /*
    #ifdef __APPLE__
        string dbUsername("root"); string dbPassword("giuliano"); string dbName("workKing");
    #else
        string dbUsername("root"); string dbPassword("root"); string dbName("catracms");
    #endif
     */
    string dbUsername = configuration["database"].get("userName", "XXX").asString();
    string dbPassword = configuration["database"].get("password", "XXX").asString();
    string dbName = configuration["database"].get("dbName", "XXX").asString();

    _maxEncodingFailures            = configuration["encoding"].get("maxEncodingFailures", 3).asInt();
    
    _confirmationCodeRetentionInDays    = configuration["mms"].get("confirmationCodeRetentionInDays", 3).asInt();
    
    shared_ptr<MySQLConnectionFactory>  mySQLConnectionFactory = 
            make_shared<MySQLConnectionFactory>(dbServer, dbUsername, dbPassword, dbName);
        
    _connectionPool = make_shared<DBConnectionPool<MySQLConnection>>(dbPoolSize, mySQLConnectionFactory);

    createTablesIfNeeded();
}

MMSEngineDBFacade::~MMSEngineDBFacade() 
{
}

vector<shared_ptr<Customer>> MMSEngineDBFacade::getCustomers()
{
    shared_ptr<MySQLConnection> conn = _connectionPool->borrow();	

    string lastSQLCommand =
        "select CustomerKey, Name, DirectoryName, MaxStorageInGB, MaxEncodingPriority from MMS_Customers where IsEnabled = 1 and CustomerType in (1, 2)";
    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
    shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());

    vector<shared_ptr<Customer>>    customers;
    
    while (resultSet->next())
    {
        shared_ptr<Customer>    customer = make_shared<Customer>();
        
        customers.push_back(customer);
        
        customer->_customerKey = resultSet->getInt("CustomerKey");
        customer->_name = resultSet->getString("Name");
        customer->_directoryName = resultSet->getString("DirectoryName");
        customer->_maxStorageInGB = resultSet->getInt("MaxStorageInGB");
        customer->_maxEncodingPriority = static_cast<int>(MMSEngineDBFacade::toEncodingPriority(resultSet->getString("MaxEncodingPriority")));

        getTerritories(customer);
    }

    _connectionPool->unborrow(conn);
    
    return customers;
}

shared_ptr<Customer> MMSEngineDBFacade::getCustomer(int64_t customerKey)
{
    shared_ptr<MySQLConnection> conn = _connectionPool->borrow();	

    string lastSQLCommand =
        "select CustomerKey, Name, DirectoryName, MaxStorageInGB, MaxEncodingPriority from MMS_Customers where CustomerKey = ?";
    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
    int queryParameterIndex = 1;
    preparedStatement->setInt64(queryParameterIndex++, customerKey);
    shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());

    shared_ptr<Customer>    customer = make_shared<Customer>();
    
    if (resultSet->next())
    {
        customer->_customerKey = resultSet->getInt("CustomerKey");
        customer->_name = resultSet->getString("Name");
        customer->_directoryName = resultSet->getString("DirectoryName");
        customer->_maxStorageInGB = resultSet->getInt("MaxStorageInGB");
        customer->_maxEncodingPriority = static_cast<int>(MMSEngineDBFacade::toEncodingPriority(resultSet->getString("MaxEncodingPriority")));

        getTerritories(customer);
    }
    else
    {
        _connectionPool->unborrow(conn);

        string errorMessage = __FILEREF__ + "select failed"
                + ", customerKey: " + to_string(customerKey)
                + ", lastSQLCommand: " + lastSQLCommand
        ;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);                    
    }

    _connectionPool->unborrow(conn);
    
    return customer;
}

shared_ptr<Customer> MMSEngineDBFacade::getCustomer(string customerName)
{
    shared_ptr<MySQLConnection> conn = _connectionPool->borrow();	

    string lastSQLCommand =
        "select CustomerKey, Name, DirectoryName, MaxStorageInGB, MaxEncodingPriority from MMS_Customers where Name = ?";
    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
    int queryParameterIndex = 1;
    preparedStatement->setString(queryParameterIndex++, customerName);
    shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());

    shared_ptr<Customer>    customer = make_shared<Customer>();
    
    if (resultSet->next())
    {
        customer->_customerKey = resultSet->getInt("CustomerKey");
        customer->_name = resultSet->getString("Name");
        customer->_directoryName = resultSet->getString("DirectoryName");
        customer->_maxStorageInGB = resultSet->getInt("MaxStorageInGB");
        customer->_maxEncodingPriority = static_cast<int>(MMSEngineDBFacade::toEncodingPriority(resultSet->getString("MaxEncodingPriority")));

        getTerritories(customer);
    }
    else
    {
        _connectionPool->unborrow(conn);

        string errorMessage = __FILEREF__ + "select failed"
                + ", customerName: " + customerName
                + ", lastSQLCommand: " + lastSQLCommand
        ;
        _logger->error(errorMessage);

        throw runtime_error(errorMessage);                    
    }

    _connectionPool->unborrow(conn);
    
    return customer;
}

void MMSEngineDBFacade::getTerritories(shared_ptr<Customer> customer)
{
    shared_ptr<MySQLConnection> conn = _connectionPool->borrow();	

    string lastSQLCommand =
        "select TerritoryKey, Name from MMS_Territories t where CustomerKey = ?";
    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(
        lastSQLCommand));
    preparedStatement->setInt(1, customer->_customerKey);
    shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());

    while (resultSet->next())
    {
        customer->_territories.insert(make_pair(resultSet->getInt("TerritoryKey"), resultSet->getString("Name")));
    }

    _connectionPool->unborrow(conn);
}

tuple<int64_t,int64_t,string> MMSEngineDBFacade::registerCustomer(
	string customerName,
    string customerDirectoryName,
	string street,
    string city,
    string state,
	string zip,
    string phone,
    string countryCode,
    CustomerType customerType,
	string deliveryURL,
	EncodingPriority maxEncodingPriority,
    EncodingPeriod encodingPeriod,
	long maxIngestionsNumber,
    long maxStorageInGB,
	string languageCode,
    string userName,
    string userPassword,
    string userEmailAddress,
    chrono::system_clock::time_point userExpirationDate
)
{
    int64_t         customerKey;
    int64_t         userKey;
    string          confirmationCode;
    int64_t         contentProviderKey;
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        
        {
            bool enabled = false;
            
            lastSQLCommand = 
                    "insert into MMS_Customers ("
                    "CustomerKey, CreationDate, Name, DirectoryName, Street, City, State, ZIP, Phone, CountryCode, CustomerType, DeliveryURL, IsEnabled, MaxEncodingPriority, EncodingPeriod, MaxIngestionsNumber, MaxStorageInGB, CurrentStorageUsageInGB, LanguageCode) values ("
                    "NULL, NULL, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, customerName);
            preparedStatement->setString(queryParameterIndex++, customerDirectoryName);
            if (street == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, street);
            if (city == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, city);
            if (state == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, state);
            if (zip == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, zip);
            if (phone == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, phone);
            if (countryCode == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, countryCode);
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(customerType));
            if (deliveryURL == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, deliveryURL);
            preparedStatement->setInt(queryParameterIndex++, enabled);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(maxEncodingPriority));
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(encodingPeriod));
            preparedStatement->setInt(queryParameterIndex++, maxIngestionsNumber);
            preparedStatement->setInt(queryParameterIndex++, maxStorageInGB);
            preparedStatement->setString(queryParameterIndex++, languageCode);

            preparedStatement->executeUpdate();
        }

        customerKey = getLastInsertId(conn);

        unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
        default_random_engine e(seed);
        confirmationCode = to_string(e());
        {
            lastSQLCommand = 
                    "insert into MMS_ConfirmationCodes (CustomerKey, CreationDate, ConfirmationCode) values ("
                    "?, NOW(), ?)";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, customerKey);
            preparedStatement->setString(queryParameterIndex++, confirmationCode);

            preparedStatement->executeUpdate();
        }

        {
            lastSQLCommand = 
                    "insert into MMS_CustomerMoreInfo (CustomerKey, CurrentDirLevel1, CurrentDirLevel2, CurrentDirLevel3, StartDateTime, EndDateTime, CurrentIngestionsNumber) values ("
                    "?, 0, 0, 0, NOW(), NOW(), 0)";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, customerKey);

            preparedStatement->executeUpdate();
        }

        {
            lastSQLCommand = 
                "insert into MMS_ContentProviders (ContentProviderKey, CustomerKey, Name) values ("
                "NULL, ?, ?)";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, customerKey);
            preparedStatement->setString(queryParameterIndex++, _defaultContentProviderName);

            preparedStatement->executeUpdate();
        }

        contentProviderKey = getLastInsertId(conn);

        int64_t territoryKey = addTerritory(
                conn,
                customerKey,
                _defaultTerritoryName);
        
        int userType = getMMSUser();
        
        userKey = addUser (
                conn,
                customerKey,
                userName,
                userPassword,
                userType,
                userEmailAddress,
                userExpirationDate);

        // insert default EncodingProfilesSet per Customer/ContentType
        {
            vector<ContentType> contentTypes = { ContentType::Video, ContentType::Audio, ContentType::Image };
            
            for (ContentType contentType: contentTypes)
            {
                {
                    lastSQLCommand = 
                        "insert into MMS_EncodingProfilesSet (EncodingProfilesSetKey, ContentType, CustomerKey, Name) values ("
                        "NULL, ?, ?, NULL)";
                    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                    preparedStatement->setInt64(queryParameterIndex++, customerKey);
                    preparedStatement->executeUpdate();
                }
                        
                int64_t encodingProfilesSetKey = getLastInsertId(conn);

		// by default this new customer will inherited the profiles associated to 'global' 
                {
                    lastSQLCommand = 
                        "insert into MMS_EncodingProfilesSetMapping (EncodingProfilesSetKey, EncodingProfileKey) " 
                        "(select ?, EncodingProfileKey from MMS_EncodingProfilesSetMapping where EncodingProfilesSetKey = " 
                            "(select EncodingProfilesSetKey from MMS_EncodingProfilesSet where ContentType = ? and CustomerKey is null and Name is null))";
                    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatement->setInt64(queryParameterIndex++, encodingProfilesSetKey);
                    preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                    preparedStatement->executeUpdate();
                }
            }
        }

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    tuple<int64_t,int64_t,string> customerKeyUserKeyAndConfirmationCode = make_tuple(customerKey, userKey, confirmationCode);
    
    return customerKeyUserKeyAndConfirmationCode;
}

void MMSEngineDBFacade::confirmCustomer(
    string confirmationCode
)
{
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

        int64_t     customerKey;
        {
            lastSQLCommand = 
                "select CustomerKey from MMS_ConfirmationCodes where ConfirmationCode = ? and DATE_ADD(CreationDate, INTERVAL ? DAY) >= NOW()";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, confirmationCode);
            preparedStatement->setInt(queryParameterIndex++, _confirmationCodeRetentionInDays);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                customerKey = resultSet->getInt64("CustomerKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "Confirmation Code not found or expired"
                    + ", confirmationCode: " + confirmationCode
                    + ", _confirmationCodeRetentionInDays: " + to_string(_confirmationCodeRetentionInDays)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        {
            bool enabled = true;
            
            lastSQLCommand = 
                "update MMS_Customers set IsEnabled = ? where CustomerKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, enabled);
            preparedStatement->setInt64(queryParameterIndex++, customerKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", enabled: " + to_string(enabled)
                        + ", customerKey: " + to_string(customerKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
}

int64_t MMSEngineDBFacade::addTerritory (
	shared_ptr<MySQLConnection> conn,
        int64_t customerKey,
        string territoryName
)
{
    int64_t         territoryKey;
    
    string      lastSQLCommand;

    try
    {
        {
            lastSQLCommand = 
                "insert into MMS_Territories (TerritoryKey, CustomerKey, Name, Currency) values ("
    		"NULL, ?, ?, ?)";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, customerKey);
            preparedStatement->setString(queryParameterIndex++, territoryName);
            string currency("");
            if (currency == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, currency);

            preparedStatement->executeUpdate();
        }
        
        territoryKey = getLastInsertId(conn);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return territoryKey;
}

int64_t MMSEngineDBFacade::addUser (
	shared_ptr<MySQLConnection> conn,
        int64_t customerKey,
        string userName,
        string password,
        int type,
        string emailAddress,
        chrono::system_clock::time_point expirationDate
)
{
    int64_t         userKey;
    
    string      lastSQLCommand;

    try
    {
        {
            lastSQLCommand = 
                "insert into MMS_Users2 (UserKey, UserName, Password, CustomerKey, Type, EMailAddress, CreationDate, ExpirationDate) values ("
                "NULL, ?, ?, ?, ?, ?, NULL, STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'))";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, userName);
            preparedStatement->setString(queryParameterIndex++, password);
            preparedStatement->setInt64(queryParameterIndex++, customerKey);
            preparedStatement->setInt(queryParameterIndex++, type);
            if (emailAddress == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, emailAddress);
            {
                tm          tmDateTime;
                char        strExpirationDate [64];
                time_t utcTime = chrono::system_clock::to_time_t(expirationDate);
                
                localtime_r (&utcTime, &tmDateTime);

                sprintf (strExpirationDate, "%04d-%02d-%02d %02d:%02d:%02d",
                        tmDateTime. tm_year + 1900,
                        tmDateTime. tm_mon + 1,
                        tmDateTime. tm_mday,
                        tmDateTime. tm_hour,
                        tmDateTime. tm_min,
                        tmDateTime. tm_sec);

                preparedStatement->setString(queryParameterIndex++, strExpirationDate);
            }
            
            preparedStatement->executeUpdate();
        }
        
        userKey = getLastInsertId(conn);
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return userKey;
}

bool MMSEngineDBFacade::isLoginValid(
        string emailAddress,
        string password
)
{
    bool        isLoginValid;
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

        {
            lastSQLCommand = 
                "select UserKey from MMS_Users2 where EMailAddress = ? and Password = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, emailAddress);
            preparedStatement->setString(queryParameterIndex++, password);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                int64_t userKey = resultSet->getInt("UserKey");
                
                isLoginValid = true;
            }
            else
            {
                isLoginValid = false;
            }            
        }
                        
        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {        
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return isLoginValid;
}

string MMSEngineDBFacade::getPassword(string emailAddress)
{
    string      password;
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

        {
            lastSQLCommand = 
                "select Password from MMS_Users2 where EMailAddress = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, emailAddress);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                password = resultSet->getString("Password");
            }
            else
            {
                string errorMessage = __FILEREF__ + "User is not present"
                    + ", emailAddress: " + emailAddress
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }
                        
        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {        
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return password;
}

string MMSEngineDBFacade::createAPIKey (
        int64_t customerKey,
        int64_t userKey,
        bool adminAPI, 
        bool userAPI,
        chrono::system_clock::time_point expirationDate
)
{
    string          apiKey;
    
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

        string emailAddress;
        {
            lastSQLCommand = 
                "select EMailAddress from MMS_Users2 where CustomerKey = ? and UserKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, customerKey);
            preparedStatement->setInt64(queryParameterIndex++, userKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                emailAddress = resultSet->getString("EMailAddress");
            }
            else
            {
                string errorMessage = __FILEREF__ + "Customer-User are not present"
                    + ", customerKey: " + to_string(customerKey)
                    + ", userKey: " + to_string(userKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }

        {
            unsigned seed = chrono::steady_clock::now().time_since_epoch().count();
            default_random_engine e(seed);

            string sourceApiKey = emailAddress + "__SEP__" + to_string(e());
            apiKey = Encrypt::encrypt(sourceApiKey);

            string flags;
            {
                if (adminAPI)
                    flags.append("ADMIN_API");

                if (userAPI)
                {
                    if (flags != "")
                       flags.append(",");
                    flags.append("USER_API");
                }
            }
            
            lastSQLCommand = 
                "insert into MMS_APIKeys (APIKey, UserKey, Flags, CreationDate, ExpirationDate) values ("
                "?, ?, ?, NULL, STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'))";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, apiKey);
            preparedStatement->setInt64(queryParameterIndex++, userKey);
            preparedStatement->setString(queryParameterIndex++, flags); // ADMIN_API,USER_API
            {
                tm          tmDateTime;
                char        strExpirationDate [64];
                time_t utcTime = chrono::system_clock::to_time_t(expirationDate);

                localtime_r (&utcTime, &tmDateTime);

                sprintf (strExpirationDate, "%04d-%02d-%02d %02d:%02d:%02d",
                        tmDateTime. tm_year + 1900,
                        tmDateTime. tm_mon + 1,
                        tmDateTime. tm_mday,
                        tmDateTime. tm_hour,
                        tmDateTime. tm_min,
                        tmDateTime. tm_sec);

                preparedStatement->setString(queryParameterIndex++, strExpirationDate);
            }

            preparedStatement->executeUpdate();
        }

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {        
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return apiKey;
}

tuple<shared_ptr<Customer>,bool,bool> MMSEngineDBFacade::checkAPIKey (string apiKey)
{
    shared_ptr<Customer> customer;
    string          flags;
    string          lastSQLCommand;

    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

        int64_t         userKey;
        
        {
            lastSQLCommand = 
                "select UserKey, Flags from MMS_APIKeys where APIKey = ? and ExpirationDate >= NOW()";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, apiKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                userKey = resultSet->getInt64("UserKey");
                flags = resultSet->getString("Flags");
            }
            else
            {
                string errorMessage = __FILEREF__ + "apiKey is not present or it is expired"
                    + ", apiKey: " + apiKey
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw APIKeyNotFoundOrExpired();
            }
        }

        int64_t                 customerKey;
        
        {
            lastSQLCommand = 
                "select c.CustomerKey from MMS_Customers c, MMS_Users2 u where c.CustomerKey = u.CustomerKey and u.UserKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, userKey);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                customerKey = resultSet->getInt64("CustomerKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "CustomerKey is not present"
                    + ", userKey: " + to_string(userKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);
            }
        }

        customer = getCustomer(customerKey);

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(APIKeyNotFoundOrExpired e)
    {        
        _connectionPool->unborrow(conn);

        string exceptionMessage(e.what());

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw e;
    }
    catch(exception e)
    {        
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    tuple<shared_ptr<Customer>,bool,bool> customerAndFlags;
    
    customerAndFlags = make_tuple(customer,
        flags.find("ADMIN_API") == string::npos ? false : true,
        flags.find("USER_API") == string::npos ? false : true
    );
            
    return customerAndFlags;
}

int64_t MMSEngineDBFacade::addVideoEncodingProfile(
        shared_ptr<Customer> customer,
        string encodingProfileSet,  // "": default Customer family, != "": named customer family
        EncodingTechnology encodingTechnology,
        string details,
        string label,
        int width,
        int height,
        string videoCodec,
        string audioCodec
)
{
    int64_t         encodingProfileKey;

    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        
        ContentType contentType = ContentType::Video;

        {
            lastSQLCommand = 
                    "insert into MMS_EncodingProfiles ("
                    "EncodingProfileKey, ContentType, Technology, Details, Label, Width, Height, VideoCodec, AudioCodec) values ("
                    "NULL, ?, ?, ?, ?, ?, ?, ?, ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(encodingTechnology));
            preparedStatement->setString(queryParameterIndex++, details);
            if (label == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setInt(queryParameterIndex++, width);
            preparedStatement->setInt(queryParameterIndex++, height);
            if (videoCodec == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, videoCodec);
            if (audioCodec == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, audioCodec);

            preparedStatement->executeUpdate();
        }
        
        encodingProfileKey = getLastInsertId(conn);
        
        int64_t encodingProfilesSetKey;
        
        if (encodingProfileSet == "")   // default Customer family
        {
            lastSQLCommand = 
                "select EncodingProfilesSetKey from MMS_EncodingProfilesSet where ContentType = ? and CustomerKey = ? and Name is null";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
            preparedStatement->setInt64(queryParameterIndex++, customer->_customerKey);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                encodingProfilesSetKey = resultSet->getInt64("EncodingProfilesSetKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "EncodingProfilesSetKey is not present"
                    + ", contentType: " + MMSEngineDBFacade::toString(contentType)
                    + ", customer->_customerKey: " + to_string(customer->_customerKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        else
        {
            lastSQLCommand = 
                "select EncodingProfilesSetKey from MMS_EncodingProfilesSet where ContentType = ? and CustomerKey = ? and Name = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
            preparedStatement->setInt64(queryParameterIndex++, customer->_customerKey);
            preparedStatement->setString(queryParameterIndex++, encodingProfileSet);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                encodingProfilesSetKey = resultSet->getInt64("EncodingProfilesSetKey");
            }
            else
            {
                lastSQLCommand = 
                    "insert into MMS_EncodingProfilesSet (EncodingProfilesSetKey, ContentType, CustomerKey, Name) values (NULL, ?, ?, ?)";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                preparedStatement->setInt64(queryParameterIndex++, customer->_customerKey);
                preparedStatement->setString(queryParameterIndex++, encodingProfileSet);
                preparedStatement->executeUpdate();
 
                encodingProfilesSetKey = getLastInsertId(conn);
            }
        }
        
        {
            lastSQLCommand = 
                "insert into MMS_EncodingProfilesSetMapping (EncodingProfilesSetKey, EncodingProfileKey)  values (?, ?)";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, encodingProfilesSetKey);
            preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);
            preparedStatement->executeUpdate();
        }

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            preparedStatement->executeUpdate();
        }
        autoCommit = true;

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return encodingProfileKey;
}

int64_t MMSEngineDBFacade::addImageEncodingProfile(
    shared_ptr<Customer> customer,
    string encodingProfileSet,
    string details,
    string label,
    int width,
    int height
)
{    
    int64_t         encodingProfileKey;

    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        
        ContentType contentType = ContentType::Image;

        {
            lastSQLCommand = 
                    "insert into MMS_EncodingProfiles ("
                    "EncodingProfileKey, ContentType, Technology, Details, Label, Width, Height, VideoCodec, AudioCodec) values ("
                    "NULL,               ?,           ?,          ?,       ?,     ?,     ?,      NULL,       NULL)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(EncodingTechnology::Image));
            preparedStatement->setString(queryParameterIndex++, details);
            if (label == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, label);
            preparedStatement->setInt(queryParameterIndex++, width);
            preparedStatement->setInt(queryParameterIndex++, height);

            preparedStatement->executeUpdate();
        }
        
        encodingProfileKey = getLastInsertId(conn);
        
        int64_t encodingProfilesSetKey;
        
        if (encodingProfileSet == "")   // default Customer family
        {
            lastSQLCommand = 
                "select EncodingProfilesSetKey from MMS_EncodingProfilesSet where ContentType = ? and CustomerKey = ? and Name is null";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
            preparedStatement->setInt64(queryParameterIndex++, customer->_customerKey);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                encodingProfilesSetKey = resultSet->getInt64("EncodingProfilesSetKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "EncodingProfilesSetKey is not present"
                    + ", contentType: " + MMSEngineDBFacade::toString(contentType)
                    + ", customer->_customerKey: " + to_string(customer->_customerKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        else
        {
            lastSQLCommand = 
                "select EncodingProfilesSetKey from MMS_EncodingProfilesSet where ContentType = ? and CustomerKey = ? and Name = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
            preparedStatement->setInt64(queryParameterIndex++, customer->_customerKey);
            preparedStatement->setString(queryParameterIndex++, encodingProfileSet);
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                encodingProfilesSetKey = resultSet->getInt64("EncodingProfilesSetKey");
            }
            else
            {
                lastSQLCommand = 
                    "insert into MMS_EncodingProfilesSet (EncodingProfilesSetKey, ContentType, CustomerKey, Name) values (NULL, ?, ?, ?)";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                preparedStatement->setInt64(queryParameterIndex++, customer->_customerKey);
                preparedStatement->setString(queryParameterIndex++, encodingProfileSet);
                preparedStatement->executeUpdate();
 
                encodingProfilesSetKey = getLastInsertId(conn);
            }
        }
        
        {
            lastSQLCommand = 
                "insert into MMS_EncodingProfilesSetMapping (EncodingProfilesSetKey, EncodingProfileKey)  values (?, ?)";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, encodingProfilesSetKey);
            preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);
            preparedStatement->executeUpdate();
        }

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            preparedStatement->executeUpdate();
        }
        autoCommit = true;

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return encodingProfileKey;
}

void MMSEngineDBFacade::getIngestionsToBeManaged(
        vector<tuple<int64_t,shared_ptr<Customer>,string,IngestionStatus>>& ingestionsToBeManaged,
        string processorMMS,
        int maxIngestionJobs)
{
    string      lastSQLCommand;
    bool        autoCommit = true;
    
    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

        // We have the Transaction because previously there was a select for update and then the update.
        // Now we have first the update and than the select. Probable the Transaction does not need, anyway I left it
        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }

        {
            lastSQLCommand = 
                "select IngestionJobKey, CustomerKey, MetaDataContent, Status from MMS_IngestionJobs where ProcessorMMS is null "
                    "and (Status = ? or (Status in (?, ?, ?, ?) and SourceBinaryTransferred = 1)) limit ? for update";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::Start_Ingestion));
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::SourceDownloadingInProgress));
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::SourceMovingInProgress));
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::SourceCopingInProgress));
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(IngestionStatus::SourceUploadingInProgress));
            preparedStatement->setInt(queryParameterIndex++, maxIngestionJobs);

            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            while (resultSet->next())
            {
                int64_t ingestionJobKey     = resultSet->getInt64("IngestionJobKey");
                int64_t customerKey         = resultSet->getInt64("CustomerKey");
                string metaDataContent      = resultSet->getString("MetaDataContent");
                IngestionStatus ingestionStatus     = MMSEngineDBFacade::toIngestionStatus(resultSet->getString("Status"));

                shared_ptr<Customer> customer = getCustomer(customerKey);
                
                tuple<int64_t,shared_ptr<Customer>,string,IngestionStatus> ingestionToBeManaged
                        = make_tuple(ingestionJobKey, customer, metaDataContent, ingestionStatus);
                
                ingestionsToBeManaged.push_back(ingestionToBeManaged);
            }
        }

        for (tuple<int64_t,shared_ptr<Customer>,string,IngestionStatus> ingestionToBeManaged:
            ingestionsToBeManaged)
        {
            lastSQLCommand = 
                "update MMS_IngestionJobs set ProcessorMMS = ? where IngestionJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, processorMMS);
            preparedStatement->setInt64(queryParameterIndex++, get<0>(ingestionToBeManaged));

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", processorMMS: " + processorMMS
                        + ", ingestionJobKey: " + to_string(get<0>(ingestionToBeManaged))
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            preparedStatement->executeUpdate();
        }
        autoCommit = true;

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }    
    catch(exception e)
    {        
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }        
}

int64_t MMSEngineDBFacade::addIngestionJob (
	int64_t customerKey,
        string metadataContent,
        IngestionType ingestionType,
        IngestionStatus ingestionStatus
)
{
    int64_t         ingestionJobKey;
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }

        {
            lastSQLCommand = 
                "select c.IsEnabled, c.CustomerType from MMS_Customers c where c.CustomerKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, customerKey);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                int isEnabled = resultSet->getInt("IsEnabled");
                int customerType = resultSet->getInt("CustomerType");
                
                if (isEnabled != 1)
                {
                    string errorMessage = __FILEREF__ + "Customer is not enabled"
                        + ", customerKey: " + to_string(customerKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);
                    
                    throw runtime_error(errorMessage);                    
                }
                else if (customerType != static_cast<int>(CustomerType::IngestionAndDelivery) &&
                        customerType != static_cast<int>(CustomerType::EncodingOnly))
                {
                    string errorMessage = __FILEREF__ + "Customer is not enabled to ingest content"
                        + ", customerKey: " + to_string(customerKey);
                        + ", customerType: " + to_string(static_cast<int>(customerType))
                        + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);
                    
                    throw runtime_error(errorMessage);                    
                }
            }
            else
            {
                string errorMessage = __FILEREF__ + "Customer is not present/configured"
                    + ", customerKey: " + to_string(customerKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }
        
        {
            lastSQLCommand = 
                "insert into MMS_IngestionJobs (IngestionJobKey, CustomerKey, MediaItemKey, MetaDataContent, IngestionType, StartIngestion, EndIngestion, DownloadingProgress, UploadingProgress, SourceBinaryTransferred, ProcessorMMS, Status, ErrorMessage) values ("
                                               "NULL,            ?,           NULL,         ?,               ?,             NULL,           NULL,         NULL,                NULL,              0,                       NULL,         ?,      NULL)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, customerKey);
            preparedStatement->setString(queryParameterIndex++, metadataContent);
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(ingestionType));
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(ingestionStatus));

            preparedStatement->executeUpdate();
        }
        
        ingestionJobKey = getLastInsertId(conn);

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            preparedStatement->executeUpdate();
        }
        autoCommit = true;

        _connectionPool->unborrow(conn);

    }
    catch(sql::SQLException se)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {        
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return ingestionJobKey;
}

void MMSEngineDBFacade::updateIngestionJob (
        int64_t ingestionJobKey,
        IngestionStatus newIngestionStatus,
        string errorMessage,
        string processorMMS
)
{
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn;

    try
    {
        string errorMessageForSQL;
        if (errorMessage == "")
            errorMessageForSQL = errorMessage;
        else
        {
            if (errorMessageForSQL.length() >= 1024)
                errorMessageForSQL.substr(0, 1024);
            else
                errorMessageForSQL = errorMessage;
        }
        
        bool finalState;

        conn = _connectionPool->borrow();	

        string prefix = "End";
        string sNewIngestionStatus = MMSEngineDBFacade::toString(newIngestionStatus);
        if (sNewIngestionStatus.compare(0, prefix.size(), prefix) == 0)
        {
            finalState			= true;
        }
        else
        {
            finalState          = false;
        }

        if (finalState)
        {
            lastSQLCommand = 
                "update MMS_IngestionJobs set Status = ?, EndIngestion = NOW(), ProcessorMMS = ?, ErrorMessage = ? where IngestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(newIngestionStatus));
            if (processorMMS == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, processorMMS);
            if (errorMessageForSQL == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, errorMessageForSQL);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", processorMMS: " + processorMMS
                        + ", errorMessageForSQL: " + errorMessageForSQL
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        else
        {
            lastSQLCommand = 
                "update MMS_IngestionJobs set Status = ?, ProcessorMMS = ?, ErrorMessage = ? where IngestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(newIngestionStatus));
            if (processorMMS == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, processorMMS);
            if (errorMessageForSQL == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, errorMessageForSQL);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", processorMMS: " + processorMMS
                        + ", errorMessageForSQL: " + errorMessageForSQL
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        _logger->info(__FILEREF__ + "IngestionJob updated successful"
            + ", newIngestionStatus: " + MMSEngineDBFacade::toString(newIngestionStatus)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            );

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }    
    catch(exception e)
    {        
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
}

void MMSEngineDBFacade::updateIngestionJob (
        int64_t ingestionJobKey,
        IngestionType ingestionType,
        IngestionStatus newIngestionStatus,
        string errorMessage,
        string processorMMS
)
{    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;

    try
    {
        string errorMessageForSQL;
        if (errorMessage == "")
            errorMessageForSQL = errorMessage;
        else
        {
            if (errorMessageForSQL.length() >= 1024)
                errorMessageForSQL.substr(0, 1024);
            else
                errorMessageForSQL = errorMessage;
        }
        
        bool finalState;

        conn = _connectionPool->borrow();	

        string prefix = "End";
        string sNewIngestionStatus = MMSEngineDBFacade::toString(newIngestionStatus);
        if (sNewIngestionStatus.compare(0, prefix.size(), prefix) == 0)
        {
            finalState			= true;
        }
        else
        {
            finalState          = false;
        }

        if (finalState)
        {
            lastSQLCommand = 
                "update MMS_IngestionJobs set IngestionType = ?, Status = ?, EndIngestion = NOW(), ProcessorMMS = ?, ErrorMessage = ? where IngestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(ingestionType));
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(newIngestionStatus));
            if (processorMMS == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, processorMMS);
            if (errorMessageForSQL == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, errorMessageForSQL);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", processorMMS: " + processorMMS
                        + ", errorMessageForSQL: " + errorMessageForSQL
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        else
        {
            lastSQLCommand = 
                "update MMS_IngestionJobs set IngestionType = ?, Status = ?, ProcessorMMS = ?, ErrorMessage = ? where IngestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(ingestionType));
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(newIngestionStatus));
            if (processorMMS == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, processorMMS);
            if (errorMessageForSQL == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, errorMessageForSQL);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", processorMMS: " + processorMMS
                        + ", errorMessageForSQL: " + errorMessageForSQL
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        _logger->info(__FILEREF__ + "IngestionJob updated successful"
            + ", newIngestionStatus: " + MMSEngineDBFacade::toString(newIngestionStatus)
            + ", ingestionJobKey: " + to_string(ingestionJobKey)
            );

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }    
    catch(exception e)
    {        
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
}

bool MMSEngineDBFacade::updateIngestionJobSourceDownloadingInProgress (
        int64_t ingestionJobKey,
        double downloadingPercentage)
{
    
    bool        toBeCancelled = false;
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

        {
            lastSQLCommand = 
                "update MMS_IngestionJobs set DownloadingProgress = ? where IngestionJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setDouble(queryParameterIndex++, downloadingPercentage);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                // we tried to update a value but the same value was already in the table,
                // in this case rowsUpdated will be 0
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", downloadingPercentage: " + to_string(downloadingPercentage)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);                    
            }
        }
        
        {
            lastSQLCommand = 
                "select Status from MMS_IngestionJobs where IngestionJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                IngestionStatus ingestionStatus = MMSEngineDBFacade::toIngestionStatus(resultSet->getString("Status"));
                
                if (ingestionStatus == IngestionStatus::End_DownloadCancelledByUser)
                    toBeCancelled = true;
            }
            else
            {
                string errorMessage = __FILEREF__ + "IngestionJob is not found"
                    + ", ingestionJobKey: " + to_string(ingestionJobKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return toBeCancelled;
}

void MMSEngineDBFacade::updateIngestionJobSourceUploadingInProgress (
        int64_t ingestionJobKey,
        double uploadingPercentage)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

        {
            lastSQLCommand = 
                "update MMS_IngestionJobs set UploadingProgress = ? where IngestionJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setDouble(queryParameterIndex++, uploadingPercentage);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                // we tried to update a value but the same value was already in the table,
                // in this case rowsUpdated will be 0
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", uploadingPercentage: " + to_string(uploadingPercentage)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);                    
            }
        }
        
        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
}

void MMSEngineDBFacade::updateIngestionJobSourceBinaryTransferred (
        int64_t ingestionJobKey,
        bool sourceBinaryTransferred)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

        {
            lastSQLCommand = 
                "update MMS_IngestionJobs set SourceBinaryTransferred = ? where IngestionJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, sourceBinaryTransferred ? 1 : 0);
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                // we tried to update a value but the same value was already in the table,
                // in this case rowsUpdated will be 0
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", sourceBinaryTransferred: " + to_string(sourceBinaryTransferred)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);                    
            }
        }
        
        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
}

void MMSEngineDBFacade::getEncodingJobs(
        bool resetToBeDone,
        string processorMMS,
        vector<shared_ptr<MMSEngineDBFacade::EncodingItem>>& encodingItems
)
{
    string      lastSQLCommand;
        
    shared_ptr<MySQLConnection> conn;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        
        if (resetToBeDone)
        {
            lastSQLCommand = 
                "update MMS_EncodingJobs set Status = ?, ProcessorMMS = null where ProcessorMMS = ? and Status = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));
            preparedStatement->setString(queryParameterIndex++, processorMMS);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::Processing));

            int rowsReset = preparedStatement->executeUpdate();            
            if (rowsReset > 0)
                _logger->warn(__FILEREF__ + "Rows (MMS_EncodingJobs) that were reset"
                    + ", rowsReset: " + to_string(rowsReset)
                );
        }
        else
        {
            int retentionDaysToReset = 7;
            
            lastSQLCommand = 
                "update MMS_EncodingJobs set Status = ?, ProcessorMMS = null where ProcessorMMS = ? and Status = ? and DATE_ADD(EncodingJobStart, INTERVAL ? DAY) <= NOW()";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));
            preparedStatement->setString(queryParameterIndex++, processorMMS);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::Processing));
            preparedStatement->setInt(queryParameterIndex++, retentionDaysToReset);

            int rowsExpired = preparedStatement->executeUpdate();            
            if (rowsExpired > 0)
                _logger->warn(__FILEREF__ + "Rows (MMS_EncodingJobs) that were expired"
                    + ", rowsExpired: " + to_string(rowsExpired)
                );
        }
        
        {
            lastSQLCommand = 
                "select EncodingJobKey, IngestionJobKey, SourcePhysicalPathKey, EncodingPriority, EncodingProfileKey from MMS_EncodingJobs " 
                "where ProcessorMMS is null and Status = ? and EncodingJobStart <= NOW() "
                "order by EncodingPriority desc, EncodingJobStart asc, FailuresNumber asc for update";
            shared_ptr<sql::PreparedStatement> preparedStatementEncoding (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatementEncoding->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));

            shared_ptr<sql::ResultSet> encodingResultSet (preparedStatementEncoding->executeQuery());
            while (encodingResultSet->next())
            {
                shared_ptr<MMSEngineDBFacade::EncodingItem> encodingItem =
                        make_shared<MMSEngineDBFacade::EncodingItem>();
                
                encodingItem->_encodingJobKey = encodingResultSet->getInt64("EncodingJobKey");
                encodingItem->_ingestionJobKey = encodingResultSet->getInt64("IngestionJobKey");
                encodingItem->_physicalPathKey = encodingResultSet->getInt64("SourcePhysicalPathKey");
                encodingItem->_encodingPriority = static_cast<EncodingPriority>(encodingResultSet->getInt("EncodingPriority"));
                encodingItem->_encodingProfileKey = encodingResultSet->getInt64("EncodingProfileKey");
                
                {
                    lastSQLCommand = 
                        "select m.CustomerKey, m.ContentType, p.MMSPartitionNumber, p.MediaItemKey, p.FileName, p.RelativePath "
                        "from MMS_MediaItems m, MMS_PhysicalPaths p where m.MediaItemKey = p.MediaItemKey and p.PhysicalPathKey = ?";
                    shared_ptr<sql::PreparedStatement> preparedStatementPhysicalPath (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementPhysicalPath->setInt64(queryParameterIndex++, encodingItem->_physicalPathKey);

                    shared_ptr<sql::ResultSet> physicalPathResultSet (preparedStatementPhysicalPath->executeQuery());
                    if (physicalPathResultSet->next())
                    {
                        encodingItem->_contentType = MMSEngineDBFacade::toContentType(physicalPathResultSet->getString("ContentType"));
                        encodingItem->_customer = getCustomer(physicalPathResultSet->getInt64("CustomerKey"));
                        encodingItem->_mmsPartitionNumber = physicalPathResultSet->getInt("MMSPartitionNumber");
                        encodingItem->_mediaItemKey = physicalPathResultSet->getInt64("MediaItemKey");
                        encodingItem->_fileName = physicalPathResultSet->getString("FileName");
                        encodingItem->_relativePath = physicalPathResultSet->getString("RelativePath");
                    }
                    else
                    {
                        string errorMessage = __FILEREF__ + "select failed, no row returned"
                                + ", encodingItem->_physicalPathKey: " + to_string(encodingItem->_physicalPathKey)
                                + ", lastSQLCommand: " + lastSQLCommand
                        ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);                    
                    }
                }

                if (encodingItem->_contentType == ContentType::Video)
                {
                    lastSQLCommand = 
                        "select DurationInMilliSeconds from MMS_VideoItems where MediaItemKey = ?";
                    shared_ptr<sql::PreparedStatement> preparedStatementVideo (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementVideo->setInt64(queryParameterIndex++, encodingItem->_mediaItemKey);

                    shared_ptr<sql::ResultSet> videoResultSet (preparedStatementVideo->executeQuery());
                    if (videoResultSet->next())
                    {
                        encodingItem->_durationInMilliSeconds = videoResultSet->getInt64("DurationInMilliSeconds");
                    }
                    else
                    {
                        string errorMessage = __FILEREF__ + "select failed, no row returned"
                                + ", encodingItem->_mediaItemKey: " + to_string(encodingItem->_mediaItemKey)
                                + ", lastSQLCommand: " + lastSQLCommand
                        ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);                    
                    }
                }
                else if (encodingItem->_contentType == ContentType::Audio)
                {
                    lastSQLCommand = 
                        "select DurationInMilliSeconds from MMS_AudioItems where MediaItemKey = ?";
                    shared_ptr<sql::PreparedStatement> preparedStatementAudio (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementAudio->setInt64(queryParameterIndex++, encodingItem->_mediaItemKey);

                    shared_ptr<sql::ResultSet> audioResultSet (preparedStatementAudio->executeQuery());
                    if (audioResultSet->next())
                    {
                        encodingItem->_durationInMilliSeconds = audioResultSet->getInt64("DurationInMilliSeconds");
                    }
                    else
                    {
                        string errorMessage = __FILEREF__ + "select failed, no row returned"
                                + ", encodingItem->_mediaItemKey: " + to_string(encodingItem->_mediaItemKey)
                                + ", lastSQLCommand: " + lastSQLCommand
                        ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);                    
                    }
                }

                {
                    lastSQLCommand = 
                        "select Technology, Details from MMS_EncodingProfiles where EncodingProfileKey = ?";
                    shared_ptr<sql::PreparedStatement> preparedStatementEncodingProfile (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementEncodingProfile->setInt64(queryParameterIndex++, encodingItem->_encodingProfileKey);

                    shared_ptr<sql::ResultSet> encodingProfilesResultSet (preparedStatementEncodingProfile->executeQuery());
                    if (encodingProfilesResultSet->next())
                    {
                        encodingItem->_encodingProfileTechnology = static_cast<EncodingTechnology>(encodingProfilesResultSet->getInt("Technology"));
                        encodingItem->_details = encodingProfilesResultSet->getString("Details");
                    }
                    else
                    {
                        string errorMessage = __FILEREF__ + "select failed"
                                + ", encodingItem->_encodingProfileKey: " + to_string(encodingItem->_encodingProfileKey)
                                + ", lastSQLCommand: " + lastSQLCommand
                        ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);                    
                    }
                }
                
                encodingItems.push_back(encodingItem);
                
                {
                    lastSQLCommand = 
                        "update MMS_EncodingJobs set Status = ?, ProcessorMMS = ?, EncodingJobStart = NULL where EncodingJobKey = ? and ProcessorMMS is null";
                    shared_ptr<sql::PreparedStatement> preparedStatementUpdateEncoding (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementUpdateEncoding->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::Processing));
                    preparedStatementUpdateEncoding->setString(queryParameterIndex++, processorMMS);
                    preparedStatementUpdateEncoding->setInt64(queryParameterIndex++, encodingItem->_encodingJobKey);

                    int rowsUpdated = preparedStatementUpdateEncoding->executeUpdate();
                    if (rowsUpdated != 1)
                    {
                        string errorMessage = __FILEREF__ + "no update was done"
                                + ", processorMMS: " + processorMMS
                                + ", encodingJobKey: " + to_string(encodingItem->_encodingJobKey)
                                + ", rowsUpdated: " + to_string(rowsUpdated)
                                + ", lastSQLCommand: " + lastSQLCommand
                        ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);                    
                    }
                }
            }
        }

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", processorMMS: " + processorMMS
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
}

int MMSEngineDBFacade::updateEncodingJob (
        int64_t encodingJobKey,
        EncodingError encodingError,
        int64_t ingestionJobKey)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;
    bool autoCommit = true;

    int encodingFailureNumber = -1;

    try
    {
        conn = _connectionPool->borrow();	

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        
        EncodingStatus newEncodingStatus;
        if (encodingError == EncodingError::PunctualError)
        {
            {
                lastSQLCommand = 
                    "select FailuresNumber from MMS_EncodingJobs where EncodingJobKey = ? for update";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                if (resultSet->next())
                {
                    encodingFailureNumber = resultSet->getInt(1);
                }
                else
                {
                    string errorMessage = __FILEREF__ + "EncodingJob not found"
                            + ", EncodingJobKey: " + to_string(encodingJobKey)
                            + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }
            }
            
            if (encodingFailureNumber + 1 >= _maxEncodingFailures)
                newEncodingStatus          = EncodingStatus::End_Failed;
            else
            {
                newEncodingStatus          = EncodingStatus::ToBeProcessed;
                encodingFailureNumber++;
            }

            {
                lastSQLCommand = 
                    "update MMS_EncodingJobs set Status = ?, ProcessorMMS = NULL, FailuresNumber = ?, EncodingProgress = NULL where EncodingJobKey = ? and Status = ?";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(newEncodingStatus));
                preparedStatement->setInt(queryParameterIndex++, encodingFailureNumber);
                preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::Processing));

                int rowsUpdated = preparedStatement->executeUpdate();
                if (rowsUpdated != 1)
                {
                    string errorMessage = __FILEREF__ + "no update was done"
                            + ", MMSEngineDBFacade::toString(newEncodingStatus): " + MMSEngineDBFacade::toString(newEncodingStatus)
                            + ", encodingJobKey: " + to_string(encodingJobKey)
                            + ", rowsUpdated: " + to_string(rowsUpdated)
                            + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }
            }
            
            _logger->info(__FILEREF__ + "EncodingJob updated successful"
                + ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus)
                + ", encodingFailureNumber: " + to_string(encodingFailureNumber)
                + ", encodingJobKey: " + to_string(encodingJobKey)
            );
        }
        else if (encodingError == EncodingError::MaxCapacityReached || encodingError == EncodingError::ErrorBeforeEncoding)
        {
            newEncodingStatus       = EncodingStatus::ToBeProcessed;
            
            lastSQLCommand = 
                "update MMS_EncodingJobs set Status = ?, ProcessorMMS = NULL, EncodingProgress = NULL where EncodingJobKey = ? and Status = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(newEncodingStatus));
                preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);
                preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::Processing));

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", MMSEngineDBFacade::toString(newEncodingStatus): " + MMSEngineDBFacade::toString(newEncodingStatus)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
            _logger->info(__FILEREF__ + "EncodingJob updated successful"
                + ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus)
                + ", encodingJobKey: " + to_string(encodingJobKey)
            );
        }
        else    // success
        {
            newEncodingStatus       = EncodingStatus::End_ProcessedSuccessful;

            lastSQLCommand = 
                "update MMS_EncodingJobs set Status = ?, ProcessorMMS = NULL, EncodingJobEnd = NOW(), EncodingProgress = 100 where EncodingJobKey = ? and Status = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(newEncodingStatus));
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::Processing));

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", MMSEngineDBFacade::toString(newEncodingStatus): " + MMSEngineDBFacade::toString(newEncodingStatus)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
            _logger->info(__FILEREF__ + "EncodingJob updated successful"
                + ", newEncodingStatus: " + MMSEngineDBFacade::toString(newEncodingStatus)
                + ", encodingJobKey: " + to_string(encodingJobKey)
            );
        }
        
        if (newEncodingStatus == EncodingStatus::End_ProcessedSuccessful || newEncodingStatus == EncodingStatus::End_Failed)
        {
            lastSQLCommand = 
                "select count(*) from MMS_EncodingJobs where IngestionJobKey = ? and (Status <> ? and Status <> ?)";
            shared_ptr<sql::PreparedStatement> preparedStatementEncoding (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatementEncoding->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatementEncoding->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_ProcessedSuccessful));
            preparedStatementEncoding->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));

            shared_ptr<sql::ResultSet> resultSetEncoding (preparedStatementEncoding->executeQuery());
            if (resultSetEncoding->next())
            {
                if (resultSetEncoding->getInt(1) == 0)  // ingestionJob is finished
                {
                    lastSQLCommand = 
                        "select count(*) from MMS_EncodingJobs where IngestionJobKey = ? and Status = ?";
                    shared_ptr<sql::PreparedStatement> preparedStatementIngestion (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementIngestion->setInt64(queryParameterIndex++, ingestionJobKey);
                    preparedStatementIngestion->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::End_Failed));

                    shared_ptr<sql::ResultSet> resultSetIngestion (preparedStatementIngestion->executeQuery());
                    if (resultSetIngestion->next())
                    {
                        IngestionStatus ingestionStatus;
                        
                        if (resultSetIngestion->getInt(1) == 0)  // no failures
                            ingestionStatus = IngestionStatus::End_IngestionSuccess;
                        else
                            ingestionStatus = IngestionStatus::End_IngestionSuccess_AtLeastOneEncodingProfileError;

                        string errorMessage = "";
                        updateIngestionJob (ingestionJobKey, ingestionStatus, errorMessage, "" /* processorMMS */);
                    }
                    else
                    {
                        string errorMessage = __FILEREF__ + "count(*) failure"
                                + ", IngestionJobKey: " + to_string(encodingJobKey)
                                + ", lastSQLCommand: " + lastSQLCommand
                        ;
                        _logger->error(errorMessage);

                        throw runtime_error(errorMessage);                    
                    }
                }
            }
            else
            {
                string errorMessage = __FILEREF__ + "EncodingJob not found"
                        + ", EncodingJobKey: " + to_string(encodingJobKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return encodingFailureNumber;
}

void MMSEngineDBFacade::updateEncodingJobProgress (
        int64_t encodingJobKey,
        int encodingPercentage)
{
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;

    try
    {
        conn = _connectionPool->borrow();	

        {
            lastSQLCommand = 
                "update MMS_EncodingJobs set EncodingProgress = ? where EncodingJobKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt(queryParameterIndex++, encodingPercentage);
            preparedStatement->setInt64(queryParameterIndex++, encodingJobKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                // probable because encodingPercentage was already the same in the table
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", encodingPercentage: " + to_string(encodingPercentage)
                        + ", encodingJobKey: " + to_string(encodingJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->warn(errorMessage);

                // throw runtime_error(errorMessage);                    
            }
        }
        
        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
}

string MMSEngineDBFacade::checkCustomerMaxIngestionNumber (
    int64_t customerKey
)
{
    string      relativePathToBeUsed;
    string      lastSQLCommand;

    shared_ptr<MySQLConnection> conn;

    try
    {
        int maxIngestionsNumber;
        int currentIngestionsNumber;
        int encodingPeriod;
        string periodStartDateTime;
        string periodEndDateTime;
        int currentDirLevel1;
        int currentDirLevel2;
        int currentDirLevel3;

        conn = _connectionPool->borrow();	

        {
            lastSQLCommand = 
                "select c.MaxIngestionsNumber, cmi.CurrentIngestionsNumber, c.EncodingPeriod, " 
                    "DATE_FORMAT(cmi.StartDateTime, '%Y-%m-%d %H:%i:%s') as LocalStartDateTime, DATE_FORMAT(cmi.EndDateTime, '%Y-%m-%d %H:%i:%s') as LocalEndDateTime, "
                    "cmi.CurrentDirLevel1, cmi.CurrentDirLevel2, cmi.CurrentDirLevel3 "
                "from MMS_Customers c, MMS_CustomerMoreInfo cmi where c.CustomerKey = cmi.CustomerKey and c.CustomerKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, customerKey);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                maxIngestionsNumber = resultSet->getInt("MaxIngestionsNumber");
                currentIngestionsNumber = resultSet->getInt("CurrentIngestionsNumber");
                encodingPeriod = resultSet->getInt("EncodingPeriod");
                periodStartDateTime = resultSet->getString("LocalStartDateTime");
                periodEndDateTime = resultSet->getString("LocalEndDateTime");                
                currentDirLevel1 = resultSet->getInt("CurrentDirLevel1");
                currentDirLevel2 = resultSet->getInt("CurrentDirLevel2");
                currentDirLevel3 = resultSet->getInt("CurrentDirLevel3");
            }
            else
            {
                string errorMessage = __FILEREF__ + "Customer is not present/configured"
                    + ", customerKey: " + to_string(customerKey)
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }
        
        bool ingestionsAllowed = true;
        bool periodExpired = false;
        char newPeriodStartDateTime [64];
        char newPeriodEndDateTime [64];

        {
            char                strDateTimeNow [64];
            tm                  tmDateTimeNow;
            chrono::system_clock::time_point now = chrono::system_clock::now();
            time_t utcTimeNow = chrono::system_clock::to_time_t(now);
            localtime_r (&utcTimeNow, &tmDateTimeNow);

            sprintf (strDateTimeNow, "%04d-%02d-%02d %02d:%02d:%02d",
                tmDateTimeNow. tm_year + 1900,
                tmDateTimeNow. tm_mon + 1,
                tmDateTimeNow. tm_mday,
                tmDateTimeNow. tm_hour,
                tmDateTimeNow. tm_min,
                tmDateTimeNow. tm_sec);

            if (periodStartDateTime.compare(strDateTimeNow) <= 0 && periodEndDateTime.compare(strDateTimeNow) >= 0)
            {
                // Period not expired

                // periodExpired = false; already initialized
                
                if (currentIngestionsNumber >= maxIngestionsNumber)
                {
                    // no more ingestions are allowed for this customer

                    ingestionsAllowed = false;
                }
            }
            else
            {
                // Period expired

                periodExpired = true;
                
                if (encodingPeriod == static_cast<int>(EncodingPeriod::Daily))
                {
                    sprintf (newPeriodStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                            tmDateTimeNow. tm_year + 1900,
                            tmDateTimeNow. tm_mon + 1,
                            tmDateTimeNow. tm_mday,
                            0,  // tmDateTimeNow. tm_hour,
                            0,  // tmDateTimeNow. tm_min,
                            0  // tmDateTimeNow. tm_sec
                    );
                    sprintf (newPeriodEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                            tmDateTimeNow. tm_year + 1900,
                            tmDateTimeNow. tm_mon + 1,
                            tmDateTimeNow. tm_mday,
                            23,  // tmCurrentDateTime. tm_hour,
                            59,  // tmCurrentDateTime. tm_min,
                            59  // tmCurrentDateTime. tm_sec
                    );
                }
                else if (encodingPeriod == static_cast<int>(EncodingPeriod::Weekly))
                {
                    // from monday to sunday
                    // monday
                    {
                        int daysToHavePreviousMonday;

                        if (tmDateTimeNow.tm_wday == 0)  // Sunday
                            daysToHavePreviousMonday = 6;
                        else
                            daysToHavePreviousMonday = tmDateTimeNow.tm_wday - 1;

                        chrono::system_clock::time_point mondayOfCurrentWeek;
                        if (daysToHavePreviousMonday != 0)
                        {
                            chrono::duration<int, ratio<60*60*24>> days(daysToHavePreviousMonday);
                            mondayOfCurrentWeek = now - days;
                        }
                        else
                            mondayOfCurrentWeek = now;

                        tm                  tmMondayOfCurrentWeek;
                        time_t utcTimeMondayOfCurrentWeek = chrono::system_clock::to_time_t(mondayOfCurrentWeek);
                        localtime_r (&utcTimeMondayOfCurrentWeek, &tmMondayOfCurrentWeek);

                        sprintf (newPeriodStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmMondayOfCurrentWeek. tm_year + 1900,
                                tmMondayOfCurrentWeek. tm_mon + 1,
                                tmMondayOfCurrentWeek. tm_mday,
                                0,  // tmDateTimeNow. tm_hour,
                                0,  // tmDateTimeNow. tm_min,
                                0  // tmDateTimeNow. tm_sec
                        );
                    }

                    // sunday
                    {
                        int daysToHaveNextSunday;

                        daysToHaveNextSunday = 7 - tmDateTimeNow.tm_wday;

                        chrono::system_clock::time_point sundayOfCurrentWeek;
                        if (daysToHaveNextSunday != 0)
                        {
                            chrono::duration<int, ratio<60*60*24>> days(daysToHaveNextSunday);
                            sundayOfCurrentWeek = now + days;
                        }
                        else
                            sundayOfCurrentWeek = now;

                        tm                  tmSundayOfCurrentWeek;
                        time_t utcTimeSundayOfCurrentWeek = chrono::system_clock::to_time_t(sundayOfCurrentWeek);
                        localtime_r (&utcTimeSundayOfCurrentWeek, &tmSundayOfCurrentWeek);

                        sprintf (newPeriodEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmSundayOfCurrentWeek. tm_year + 1900,
                                tmSundayOfCurrentWeek. tm_mon + 1,
                                tmSundayOfCurrentWeek. tm_mday,
                                23,  // tmSundayOfCurrentWeek. tm_hour,
                                59,  // tmSundayOfCurrentWeek. tm_min,
                                59  // tmSundayOfCurrentWeek. tm_sec
                        );
                    }
                }
                else if (encodingPeriod == static_cast<int>(EncodingPeriod::Monthly))
                {
                    // first day of the month
                    {
                        sprintf (newPeriodStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmDateTimeNow. tm_year + 1900,
                                tmDateTimeNow. tm_mon + 1,
                                1,  // tmDateTimeNow. tm_mday,
                                0,  // tmDateTimeNow. tm_hour,
                                0,  // tmDateTimeNow. tm_min,
                                0  // tmDateTimeNow. tm_sec
                        );
                    }

                    // last day of the month
                    {
                        tm                  tmLastDayOfCurrentMonth = tmDateTimeNow;

                        tmLastDayOfCurrentMonth.tm_mday = 1;

                        // Next month 0=Jan
                        if (tmLastDayOfCurrentMonth.tm_mon == 11)    // Dec
                        {
                            tmLastDayOfCurrentMonth.tm_mon = 0;
                            tmLastDayOfCurrentMonth.tm_year++;
                        }
                        else
                        {
                            tmLastDayOfCurrentMonth.tm_mon++;
                        }

                        // Get the first day of the next month
                        time_t utcTimeLastDayOfCurrentMonth = mktime (&tmLastDayOfCurrentMonth);

                        // Subtract 1 day
                        utcTimeLastDayOfCurrentMonth -= 86400;

                        // Convert back to date and time
                        localtime_r (&utcTimeLastDayOfCurrentMonth, &tmLastDayOfCurrentMonth);

                        sprintf (newPeriodEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmLastDayOfCurrentMonth. tm_year + 1900,
                                tmLastDayOfCurrentMonth. tm_mon + 1,
                                tmLastDayOfCurrentMonth. tm_mday,
                                23,  // tmDateTimeNow. tm_hour,
                                59,  // tmDateTimeNow. tm_min,
                                59  // tmDateTimeNow. tm_sec
                        );
                    }
                }
                else // if (encodingPeriod == static_cast<int>(EncodingPeriod::Yearly))
                {
                    // first day of the year
                    {
                        sprintf (newPeriodStartDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmDateTimeNow. tm_year + 1900,
                                1,  // tmDateTimeNow. tm_mon + 1,
                                1,  // tmDateTimeNow. tm_mday,
                                0,  // tmDateTimeNow. tm_hour,
                                0,  // tmDateTimeNow. tm_min,
                                0  // tmDateTimeNow. tm_sec
                        );
                    }

                    // last day of the month
                    {
                        sprintf (newPeriodEndDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmDateTimeNow. tm_year + 1900,
                                12, // tmDateTimeNow. tm_mon + 1,
                                31, // tmDateTimeNow. tm_mday,
                                23,  // tmDateTimeNow. tm_hour,
                                59,  // tmDateTimeNow. tm_min,
                                59  // tmDateTimeNow. tm_sec
                        );
                    }
                }
            }
        }
        
        if (periodExpired)
        {
            lastSQLCommand = 
                "update MMS_CustomerMoreInfo set CurrentIngestionsNumber = 0, "
                "StartDateTime = STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'), EndDateTime = STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S') "
                "where CustomerKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setString(queryParameterIndex++, newPeriodStartDateTime);
            preparedStatement->setString(queryParameterIndex++, newPeriodEndDateTime);
            preparedStatement->setInt64(queryParameterIndex++, customerKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", newPeriodStartDateTime: " + newPeriodStartDateTime
                        + ", newPeriodEndDateTime: " + newPeriodEndDateTime
                        + ", customerKey: " + to_string(customerKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }
        }
        
        if (!ingestionsAllowed)
        {
            string errorMessage = __FILEREF__ + "Reached the max number of Ingestions in your period"
                + ", maxIngestionsNumber: " + to_string(maxIngestionsNumber)
                + ", encodingPeriod: " + to_string(static_cast<int>(encodingPeriod))
            ;
            _logger->error(errorMessage);
            
            throw runtime_error(errorMessage);
        }
        
        {
            char pCurrentRelativePath [64];
            
            sprintf (pCurrentRelativePath, "/%03d/%03d/%03d/", 
                currentDirLevel1, currentDirLevel2, currentDirLevel3);
            
            relativePathToBeUsed = pCurrentRelativePath;
        }
        
        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }    
    catch(runtime_error e)
    {
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "exception"
            + ", e.what: " + e.what()
        );

        throw e;
    }    
    catch(exception e)
    {        
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }    
    
    return relativePathToBeUsed;
}

pair<int64_t,int64_t> MMSEngineDBFacade::saveIngestedContentMetadata(
        shared_ptr<Customer> customer,
        int64_t ingestionJobKey,
        Json::Value metadataRoot,
        string relativePath,
        string mediaSourceFileName,
        int mmsPartitionIndexUsed,
        unsigned long sizeInBytes,
        int64_t videoOrAudioDurationInMilliSeconds,
        int imageWidth,
        int imageHeight
)
{
    pair<int64_t,int64_t> mediaItemKeyAndPhysicalPathKey;
    
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }

        Json::Value contentIngestion = metadataRoot["ContentIngestion"]; 

        _logger->info(__FILEREF__ + "Retrieving contentProviderKey...");
        // get ContentProviderKey
        int64_t contentProviderKey;
        {
            string contentProviderName;
            
            if (isMetadataPresent(contentIngestion, "ContentProviderName"))
                contentProviderName = contentIngestion.get("ContentProviderName", "XXX").asString();
            else
                contentProviderName = _defaultContentProviderName;

            lastSQLCommand = 
                "select ContentProviderKey from MMS_ContentProviders where CustomerKey = ? and Name = ?";
            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, customer->_customerKey);
            preparedStatement->setString(queryParameterIndex++, contentProviderName);
            
            shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
            if (resultSet->next())
            {
                contentProviderKey = resultSet->getInt64("ContentProviderKey");
            }
            else
            {
                string errorMessage = __FILEREF__ + "ContentProvider is not present"
                    + ", customer->_customerKey: " + to_string(customer->_customerKey)
                    + ", contentProviderName: " + contentProviderName
                    + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }

        _logger->info(__FILEREF__ + "Insert into MMS_MediaItems...");
        ContentType contentType;
        int64_t encodingProfileSetKey;
        {
            string title = "";
            string subTitle = "";
            string ingester = "";
            string keywords = "";
            string description = "";
            string sContentType;
            string logicalType = "";
            string encodingProfilesSet;

            title = contentIngestion.get("Title", "XXX").asString();
            
            if (isMetadataPresent(contentIngestion, "SubTitle"))
                subTitle = contentIngestion.get("SubTitle", "XXX").asString();

            if (isMetadataPresent(contentIngestion, "Ingester"))
                ingester = contentIngestion.get("Ingester", "XXX").asString();

            if (isMetadataPresent(contentIngestion, "Keywords"))
                keywords = contentIngestion.get("Keywords", "XXX").asString();

            if (isMetadataPresent(contentIngestion, "Description"))
                description = contentIngestion.get("Description", "XXX").asString();

            sContentType = contentIngestion.get("ContentType", "XXX").asString();
            try
            {
                contentType = MMSEngineDBFacade::toContentType(sContentType);
            }
            catch(exception e)
            {
                string errorMessage = __FILEREF__ + "ContentType is wrong"
                    + ", sContentType: " + sContentType
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            

            if (isMetadataPresent(contentIngestion, "LogicalType"))
                logicalType = contentIngestion.get("LogicalType", "XXX").asString();

            {
                encodingProfilesSet = contentIngestion.get("EncodingProfilesSet", "XXX").asString();
                if (encodingProfilesSet == "systemDefault")
                {
                    lastSQLCommand = 
                        "select EncodingProfilesSetKey from MMS_EncodingProfilesSet where ContentType = ? and CustomerKey is null and Name is null";
                    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                }
                else if (encodingProfilesSet == "customerDefault")
                {
                    lastSQLCommand = 
                        "select EncodingProfilesSetKey from MMS_EncodingProfilesSet where ContentType = ? and CustomerKey = ? and Name is null";
                    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                    preparedStatement->setInt64(queryParameterIndex++, customer->_customerKey);
                }
                else
                {
                    lastSQLCommand = 
                        "select EncodingProfilesSetKey from MMS_EncodingProfilesSet where ContentType = ? and CustomerKey = ? and Name = ?";
                }
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                if (encodingProfilesSet == "systemDefault")
                {
                    preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                }
                else if (encodingProfilesSet == "customerDefault")
                {
                    preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                    preparedStatement->setInt64(queryParameterIndex++, customer->_customerKey);
                }
                else
                {
                    preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                    preparedStatement->setInt64(queryParameterIndex++, customer->_customerKey);
                    preparedStatement->setString(queryParameterIndex++, encodingProfilesSet);
                }
                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                if (resultSet->next())
                {
                    encodingProfileSetKey = resultSet->getInt64("EncodingProfilesSetKey");
                }
                else
                {
                    string errorMessage = __FILEREF__ + "EncodingProfilesSetKey is not present"
                        + ", contentType: " + MMSEngineDBFacade::toString(contentType)
                        + ", customer->_customerKey: " + to_string(customer->_customerKey)
                        + ", encodingProfilesSet: " + encodingProfilesSet
                        + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }            
            }


            lastSQLCommand = 
                "insert into MMS_MediaItems (MediaItemKey, CustomerKey, ContentProviderKey, GenreKey, Title, SubTitle, Ingester, Keywords, Description, " 
                "IngestionDate, ContentType, LogicalType, EncodingProfilesSetKey) values ("
                "NULL, ?, ?, NULL, ?, ?, ?, ?, ?, NULL, ?, ?, ?)";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, customer->_customerKey);
            preparedStatement->setInt64(queryParameterIndex++, contentProviderKey);
            preparedStatement->setString(queryParameterIndex++, title);
            if (subTitle == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, subTitle);
            if (ingester == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, ingester);
            if (keywords == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, keywords);
            if (description == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, description);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
            if (logicalType == "")
                preparedStatement->setNull(queryParameterIndex++, sql::DataType::VARCHAR);
            else
                preparedStatement->setString(queryParameterIndex++, logicalType);
            preparedStatement->setInt64(queryParameterIndex++, encodingProfileSetKey);

            preparedStatement->executeUpdate();
        }
        
        int64_t mediaItemKey = getLastInsertId(conn);

        {
            if (contentType == ContentType::Video)
            {
                lastSQLCommand = 
                    "insert into MMS_VideoItems (MediaItemKey, DurationInMilliSeconds) values ("
                    "?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
                if (videoOrAudioDurationInMilliSeconds == -1)
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
                else
                    preparedStatement->setInt64(queryParameterIndex++, videoOrAudioDurationInMilliSeconds);

                preparedStatement->executeUpdate();
            }
            else if (contentType == ContentType::Audio)
            {
                lastSQLCommand = 
                    "insert into MMS_AudioItems (MediaItemKey, DurationInMilliSeconds) values ("
                    "?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
                if (videoOrAudioDurationInMilliSeconds == -1)
                    preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);
                else
                    preparedStatement->setInt64(queryParameterIndex++, videoOrAudioDurationInMilliSeconds);

                preparedStatement->executeUpdate();
            }
            else if (contentType == ContentType::Image)
            {
                lastSQLCommand = 
                    "insert into MMS_ImageItems (MediaItemKey, Width, Height) values ("
                    "?, ?, ?)";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
                preparedStatement->setInt64(queryParameterIndex++, imageWidth);
                preparedStatement->setInt64(queryParameterIndex++, imageHeight);

                preparedStatement->executeUpdate();
            }
            else
            {
                string errorMessage = __FILEREF__ + "ContentType is wrong"
                    + ", contentType: " + MMSEngineDBFacade::toString(contentType)
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }            
        }        

        {
            int drm = 0;

            lastSQLCommand = 
                "insert into MMS_PhysicalPaths(PhysicalPathKey, MediaItemKey, DRM, FileName, RelativePath, MMSPartitionNumber, SizeInBytes, EncodingProfileKey, CreationDate) values ("
		"NULL, ?, ?, ?, ?, ?, ?, ?, NOW())";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
            preparedStatement->setInt(queryParameterIndex++, drm);
            preparedStatement->setString(queryParameterIndex++, mediaSourceFileName);
            preparedStatement->setString(queryParameterIndex++, relativePath);
            preparedStatement->setInt(queryParameterIndex++, mmsPartitionIndexUsed);
            preparedStatement->setInt64(queryParameterIndex++, sizeInBytes);
            preparedStatement->setNull(queryParameterIndex++, sql::DataType::BIGINT);

            preparedStatement->executeUpdate();
        }

        int64_t physicalPathKey = getLastInsertId(conn);

        // territories
        {
            string field = "Territories";
            if (isMetadataPresent(contentIngestion, field))
            {
                const Json::Value territories = contentIngestion[field];
                
                lastSQLCommand = 
                    "select TerritoryKey, Name from MMS_Territories where CustomerKey = ?";
                shared_ptr<sql::PreparedStatement> preparedStatementTerrirories (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatementTerrirories->setInt64(queryParameterIndex++, customer->_customerKey);

                shared_ptr<sql::ResultSet> resultSetTerritories (preparedStatementTerrirories->executeQuery());
                while (resultSetTerritories->next())
                {
                    int64_t territoryKey = resultSetTerritories->getInt64("TerritoryKey");
                    string territoryName = resultSetTerritories->getString("Name");

                    string startPublishing = "NOW";
                    string endPublishing = "FOREVER";
                    if (isMetadataPresent(territories, territoryName))
                    {
                        Json::Value territory = territories[territoryName];
                        
                        field = "startPublishing";
                        if (isMetadataPresent(territory, field))
                            startPublishing = territory.get(field, "XXX").asString();

                        field = "endPublishing";
                        if (isMetadataPresent(territory, field))
                            endPublishing = territory.get(field, "XXX").asString();
                    }
                    
                    if (startPublishing == "NOW")
                    {
                        tm          tmDateTime;
                        char        strDateTime [64];

                        chrono::system_clock::time_point now = chrono::system_clock::now();
                        time_t utcTime = chrono::system_clock::to_time_t(now);
                        
                        localtime_r (&utcTime, &tmDateTime);

                        sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmDateTime. tm_year + 1900,
                                tmDateTime. tm_mon + 1,
                                tmDateTime. tm_mday,
                                tmDateTime. tm_hour,
                                tmDateTime. tm_min,
                                tmDateTime. tm_sec);

                        startPublishing = strDateTime;
                    }
                    
                    if (endPublishing == "FOREVER")
                    {
                        tm          tmDateTime;
                        char        strDateTime [64];

                        chrono::system_clock::time_point forever = chrono::system_clock::now() + chrono::hours(24 * 365 * 20);
                        
                        time_t utcTime = chrono::system_clock::to_time_t(forever);
                        
                        localtime_r (&utcTime, &tmDateTime);

                        sprintf (strDateTime, "%04d-%02d-%02d %02d:%02d:%02d",
                                tmDateTime. tm_year + 1900,
                                tmDateTime. tm_mon + 1,
                                tmDateTime. tm_mday,
                                tmDateTime. tm_hour,
                                tmDateTime. tm_min,
                                tmDateTime. tm_sec);

                        endPublishing = strDateTime;
                    }
                    
                    {
                        lastSQLCommand = 
                            "insert into MMS_DefaultTerritoryInfo(DefaultTerritoryInfoKey, MediaItemKey, TerritoryKey, StartPublishing, EndPublishing) values ("
                            "NULL, ?, ?, STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'), STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'))";

                        shared_ptr<sql::PreparedStatement> preparedStatementDefaultTerritory (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementDefaultTerritory->setInt64(queryParameterIndex++, mediaItemKey);
                        preparedStatementDefaultTerritory->setInt(queryParameterIndex++, territoryKey);
                        preparedStatementDefaultTerritory->setString(queryParameterIndex++, startPublishing);
                        preparedStatementDefaultTerritory->setString(queryParameterIndex++, endPublishing);

                        preparedStatementDefaultTerritory->executeUpdate();
                    }
                }                
            }
        }
        
        {
            int currentDirLevel1;
            int currentDirLevel2;
            int currentDirLevel3;

            {
                lastSQLCommand = 
                    "select CurrentDirLevel1, CurrentDirLevel2, CurrentDirLevel3 "
                    "from MMS_CustomerMoreInfo where CustomerKey = ?";
                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt64(queryParameterIndex++, customer->_customerKey);

                shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                if (resultSet->next())
                {
                    currentDirLevel1 = resultSet->getInt("CurrentDirLevel1");
                    currentDirLevel2 = resultSet->getInt("CurrentDirLevel2");
                    currentDirLevel3 = resultSet->getInt("CurrentDirLevel3");
                }
                else
                {
                    string errorMessage = __FILEREF__ + "Customer is not present/configured"
                        + ", customer->_customerKey: " + to_string(customer->_customerKey)
                        + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }            
            }

            if (currentDirLevel3 >= 999)
            {
                currentDirLevel3		= 0;

                if (currentDirLevel2 >= 999)
                {
                    currentDirLevel2		= 0;

                    if (currentDirLevel1 >= 999)
                    {
                        currentDirLevel1		= 0;
                    }
                    else
                    {
                        currentDirLevel1++;
                    }
                }
                else
                {
                    currentDirLevel2++;
                }
            }
            else
            {
                currentDirLevel3++;
            }

            {
                lastSQLCommand = 
                    "update MMS_CustomerMoreInfo set CurrentDirLevel1 = ?, CurrentDirLevel2 = ?, "
                    "CurrentDirLevel3 = ?, CurrentIngestionsNumber = CurrentIngestionsNumber + 1 "
                    "where CustomerKey = ?";

                shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatement->setInt(queryParameterIndex++, currentDirLevel1);
                preparedStatement->setInt(queryParameterIndex++, currentDirLevel2);
                preparedStatement->setInt(queryParameterIndex++, currentDirLevel3);
                preparedStatement->setInt64(queryParameterIndex++, customer->_customerKey);

                int rowsUpdated = preparedStatement->executeUpdate();
                if (rowsUpdated != 1)
                {
                    string errorMessage = __FILEREF__ + "no update was done"
                            + ", currentDirLevel1: " + to_string(currentDirLevel1)
                            + ", currentDirLevel2: " + to_string(currentDirLevel2)
                            + ", currentDirLevel3: " + to_string(currentDirLevel3)
                            + ", customer->_customerKey: " + to_string(customer->_customerKey)
                            + ", rowsUpdated: " + to_string(rowsUpdated)
                            + ", lastSQLCommand: " + lastSQLCommand
                    ;
                    _logger->error(errorMessage);

                    throw runtime_error(errorMessage);                    
                }
            }
        }
        
        {
            EncodingPriority encodingPriority;
            if (contentType == ContentType::Video || contentType == ContentType::Audio)
            {
                string field = "EncodingPriority";
                if (isMetadataPresent(contentIngestion, field))
                {
                    string strEncodingPriority = contentIngestion.get(field, "XXX").asString();
                    encodingPriority = MMSEngineDBFacade::toEncodingPriority(strEncodingPriority);

                    if (static_cast<int>(encodingPriority) > customer->_maxEncodingPriority)
                        encodingPriority = static_cast<EncodingPriority>(customer->_maxEncodingPriority); 
                }
                else
                    encodingPriority = static_cast<EncodingPriority>(customer->_maxEncodingPriority);
            }
            else
                encodingPriority = EncodingPriority::Medium;

            lastSQLCommand = 
                "insert into MMS_EncodingJobs(EncodingJobKey, IngestionJobKey, SourcePhysicalPathKey, EncodingPriority, EncodingProfileKey, EncodingJobStart, EncodingJobEnd, EncodingProgress, Status, ProcessorMMS, FailuresNumber) "
	        "select                       NULL,           ?,               ?,                     ?,                EncodingProfileKey, NULL,             NULL,           NULL,     ?,      NULL,         0 "
                "from MMS_EncodingProfilesSetMapping where EncodingProfilesSetKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);
            preparedStatement->setInt64(queryParameterIndex++, physicalPathKey);
            preparedStatement->setInt(queryParameterIndex++, static_cast<int>(encodingPriority));
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(EncodingStatus::ToBeProcessed));
            preparedStatement->setInt64(queryParameterIndex++, encodingProfileSetKey);

            preparedStatement->executeUpdate();
        }

        {
            IngestionStatus newIngestionStatus = IngestionStatus::QueuedForEncoding;
            
            lastSQLCommand = 
                "update MMS_IngestionJobs set MediaItemKey = ?, Status = ? where IngestionJobKey = ?";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
            preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(newIngestionStatus));
            preparedStatement->setInt64(queryParameterIndex++, ingestionJobKey);

            int rowsUpdated = preparedStatement->executeUpdate();
            if (rowsUpdated != 1)
            {
                string errorMessage = __FILEREF__ + "no update was done"
                        + ", mediaItemKey: " + to_string(mediaItemKey)
                        + ", ingestionJobKey: " + to_string(ingestionJobKey)
                        + ", rowsUpdated: " + to_string(rowsUpdated)
                        + ", lastSQLCommand: " + lastSQLCommand
                ;
                _logger->error(errorMessage);

                throw runtime_error(errorMessage);                    
            }

            _logger->info(__FILEREF__ + "Update IngestionJobs"
                + ", ingestionJobKey: " + to_string(ingestionJobKey)
                + ", newIngestionStatus: " + MMSEngineDBFacade::toString(newIngestionStatus)
            );
        }

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _connectionPool->unborrow(conn);
        
        mediaItemKeyAndPhysicalPathKey.first = mediaItemKey;
        mediaItemKeyAndPhysicalPathKey.second = physicalPathKey;
    }
    catch(sql::SQLException se)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return mediaItemKeyAndPhysicalPathKey;
}

int64_t MMSEngineDBFacade::saveEncodedContentMetadata(
        int64_t customerKey,
        int64_t mediaItemKey,
        string encodedFileName,
        string relativePath,
        int mmsPartitionIndexUsed,
        unsigned long long sizeInBytes,
        int64_t encodingProfileKey
)
{
    int64_t     encodedPhysicalPathKey;
    string      lastSQLCommand;
    
    shared_ptr<MySQLConnection> conn;
    bool autoCommit = true;

    try
    {
        conn = _connectionPool->borrow();	

        autoCommit = false;
        // conn->_sqlConnection->setAutoCommit(autoCommit); OR execute the statement START TRANSACTION
        {
            lastSQLCommand = 
                "START TRANSACTION";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        
        {
            int drm = 0;

            lastSQLCommand = 
                "insert into MMS_PhysicalPaths(PhysicalPathKey, MediaItemKey, DRM, FileName, RelativePath, MMSPartitionNumber, SizeInBytes, EncodingProfileKey, CreationDate) values ("
        	"NULL, ?, ?, ?, ?, ?, ?, ?, NOW())";

            shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatement->setInt64(queryParameterIndex++, mediaItemKey);
            preparedStatement->setInt(queryParameterIndex++, drm);
            preparedStatement->setString(queryParameterIndex++, encodedFileName);
            preparedStatement->setString(queryParameterIndex++, relativePath);
            preparedStatement->setInt(queryParameterIndex++, mmsPartitionIndexUsed);
            preparedStatement->setInt64(queryParameterIndex++, sizeInBytes);
            preparedStatement->setInt64(queryParameterIndex++, encodingProfileKey);

            preparedStatement->executeUpdate();
        }

        encodedPhysicalPathKey = getLastInsertId(conn);

        // publishing territories
        {
            lastSQLCommand = 
                "select t.TerritoryKey, t.Name, DATE_FORMAT(d.StartPublishing, '%Y-%m-%d %H:%i:%s') as StartPublishing, DATE_FORMAT(d.EndPublishing, '%Y-%m-%d %H:%i:%s') as EndPublishing from MMS_Territories t, MMS_DefaultTerritoryInfo d "
                "where t.TerritoryKey = d.TerritoryKey and t.CustomerKey = ? and d.MediaItemKey = ?";
            shared_ptr<sql::PreparedStatement> preparedStatementTerritory (conn->_sqlConnection->prepareStatement(lastSQLCommand));
            int queryParameterIndex = 1;
            preparedStatementTerritory->setInt64(queryParameterIndex++, customerKey);
            preparedStatementTerritory->setInt64(queryParameterIndex++, mediaItemKey);

            shared_ptr<sql::ResultSet> resultSetTerritory (preparedStatementTerritory->executeQuery());
            while (resultSetTerritory->next())
            {
                int64_t territoryKey = resultSetTerritory->getInt64("TerritoryKey");
                string territoryName = resultSetTerritory->getString("Name");
                string startPublishing = resultSetTerritory->getString("StartPublishing");
                string endPublishing = resultSetTerritory->getString("EndPublishing");
                
                lastSQLCommand = 
                    "select PublishingStatus from MMS_Publishing2 where MediaItemKey = ? and TerritoryKey = ?";
                shared_ptr<sql::PreparedStatement> preparedStatementPublishing (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                int queryParameterIndex = 1;
                preparedStatementPublishing->setInt64(queryParameterIndex++, mediaItemKey);
                preparedStatementPublishing->setInt64(queryParameterIndex++, territoryKey);

                shared_ptr<sql::ResultSet> resultSetPublishing (preparedStatementPublishing->executeQuery());
                if (resultSetPublishing->next())
                {
                    int publishingStatus = resultSetPublishing->getInt("PublishingStatus");
                    
                    if (publishingStatus == 1)
                    {
                        lastSQLCommand = 
                            "update MMS_Publishing2 set PublishingStatus = 0 where MediaItemKey = ? and TerritoryKey = ?";

                        shared_ptr<sql::PreparedStatement> preparedStatementUpdatePublishing (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                        int queryParameterIndex = 1;
                        preparedStatementUpdatePublishing->setInt64(queryParameterIndex++, mediaItemKey);
                        preparedStatementUpdatePublishing->setInt(queryParameterIndex++, territoryKey);

                        int rowsUpdated = preparedStatementUpdatePublishing->executeUpdate();
                        if (rowsUpdated != 1)
                        {
                            string errorMessage = __FILEREF__ + "no update was done"
                                    + ", mediaItemKey: " + to_string(mediaItemKey)
                                    + ", territoryKey: " + to_string(territoryKey)
                                    + ", rowsUpdated: " + to_string(rowsUpdated)
                                    + ", lastSQLCommand: " + lastSQLCommand
                            ;
                            _logger->error(errorMessage);

                            throw runtime_error(errorMessage);                    
                        }
                    }
                }
                else
                {
                    lastSQLCommand = 
                        "insert into MMS_Publishing2 (PublishingKey, MediaItemKey, TerritoryKey, StartPublishing, EndPublishing, PublishingStatus) values ("
	        	"NULL, ?, ?, STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'), STR_TO_DATE(?, '%Y-%m-%d %H:%i:%S'), 0)";

                    shared_ptr<sql::PreparedStatement> preparedStatementInsertPublishing (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatementInsertPublishing->setInt64(queryParameterIndex++, mediaItemKey);
                    preparedStatementInsertPublishing->setInt(queryParameterIndex++, territoryKey);
                    preparedStatementInsertPublishing->setString(queryParameterIndex++, startPublishing);
                    preparedStatementInsertPublishing->setString(queryParameterIndex++, endPublishing);

                    preparedStatementInsertPublishing->executeUpdate();
                }
            }                
        }

        // conn->_sqlConnection->commit(); OR execute COMMIT
        {
            lastSQLCommand = 
                "COMMIT";

            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute(lastSQLCommand);
        }
        autoCommit = true;

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);

        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {
        // conn->_sqlConnection->rollback(); OR execute ROLLBACK
        if (!autoCommit)
        {
            shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());
            statement->execute("ROLLBACK");
        }
        
        _connectionPool->unborrow(conn);
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return encodedPhysicalPathKey;
}

bool MMSEngineDBFacade::isMetadataPresent(Json::Value root, string field)
{
    if (root.isObject() && root.isMember(field) && !root[field].isNull()
)
        return true;
    else
        return false;
}

int64_t MMSEngineDBFacade::getLastInsertId(shared_ptr<MySQLConnection> conn)
{
    int64_t         lastInsertId;
    
    string      lastSQLCommand;

    try
    {
        lastSQLCommand = 
            "select LAST_INSERT_ID()";
        shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
        shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());

        if (resultSet->next())
        {
            lastInsertId = resultSet->getInt64(1);
        }
        else
        {
            string error ("select LAST_INSERT_ID failed");
            
            _logger->error(error);
            
            throw runtime_error(error);
        }
    }
    catch(sql::SQLException se)
    {
        string exceptionMessage(se.what());
        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", exceptionMessage: " + exceptionMessage
        );

        throw se;
    }
    catch(exception e)
    {        
        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
        );

        throw e;
    }
    
    return lastInsertId;
}

void MMSEngineDBFacade::createTablesIfNeeded()
{
    shared_ptr<MySQLConnection> conn;

    string      lastSQLCommand;

    try
    {
        conn = _connectionPool->borrow();	

        shared_ptr<sql::Statement> statement (conn->_sqlConnection->createStatement());

        try
        {
            // MaxEncodingPriority (0: low, 1: default, 2: high)
            // CustomerType: (0: Live Sessions only, 1: Ingestion + Delivery, 2: Encoding Only)
            // EncodingPeriod: 0: Daily, 1: Weekly, 2: Monthly

            lastSQLCommand = 
                "create table if not exists MMS_Customers ("
                    "CustomerKey                    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "CreationDate                   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "Name                           VARCHAR (64) NOT NULL,"
                    "DirectoryName                  VARCHAR (64) NOT NULL,"
                    "Street                         VARCHAR (128) NULL,"
                    "City                           VARCHAR (64) NULL,"
                    "State                          VARCHAR (64) NULL,"
                    "ZIP                            VARCHAR (32) NULL,"
                    "Phone                          VARCHAR (32) NULL,"
                    "CountryCode                    VARCHAR (64) NULL,"
                    "CustomerType                   TINYINT NOT NULL,"
                    "DeliveryURL                    VARCHAR (256) NULL,"
                    "IsEnabled                      TINYINT (1) NOT NULL,"
                    "MaxEncodingPriority            VARCHAR (32) NOT NULL,"
                    "EncodingPeriod                 TINYINT NOT NULL,"
                    "MaxIngestionsNumber            INT NOT NULL,"
                    "MaxStorageInGB                 INT NOT NULL,"
                    "CurrentStorageUsageInGB        INT DEFAULT 0,"
                    "SuperDeliveryRights            INT DEFAULT 0,"
                    "LanguageCode                   VARCHAR (16) NOT NULL,"
                    "constraint MMS_Customers_PK PRIMARY KEY (CustomerKey),"
                    "UNIQUE (Name))"
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);    
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            lastSQLCommand = 
                "create unique index MMS_Customers_idx on MMS_Customers (DirectoryName)";
            statement->execute(lastSQLCommand);    
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_ConfirmationCodes ("
                    "CustomerKey                    BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "CreationDate                   TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "ConfirmationCode               VARCHAR (64) NOT NULL,"
                    "constraint MMS_ConfirmationCodes_PK PRIMARY KEY (CustomerKey),"
                    "UNIQUE (ConfirmationCode))"
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);    
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // The territories are present only if the Customer is a 'Content Provider'.
            // In this case we could have two scenarios:
            // - customer not having territories (we will have just one row in this table with Name set as 'default')
            // - customer having at least one territory (we will as much rows in this table according the number of territories)
            lastSQLCommand = 
                "create table if not exists MMS_Territories ("
                    "TerritoryKey  				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "CustomerKey  				BIGINT UNSIGNED NOT NULL,"
                    "Name					VARCHAR (64) NOT NULL,"
                    "Currency					VARCHAR (16) DEFAULT NULL,"
                    "constraint MMS_Territories_PK PRIMARY KEY (TerritoryKey),"
                    "constraint MMS_Territories_FK foreign key (CustomerKey) "
                        "references MMS_Customers (CustomerKey) on delete cascade, "
                    "UNIQUE (CustomerKey, Name))"
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            // create table MMS_CustomerMoreInfo. This table was created to move the fields
            //		that are updated during the ingestion from MMS_Customers.
            //		That will avoid to put a lock in the MMS_Customers during the update
            //		since the MMS_Customers is a wide used table
            lastSQLCommand = 
                "create table if not exists MMS_CustomerMoreInfo ("
                    "CustomerKey  			BIGINT UNSIGNED NOT NULL,"
                    "CurrentDirLevel1			INT NOT NULL,"
                    "CurrentDirLevel2			INT NOT NULL,"
                    "CurrentDirLevel3			INT NOT NULL,"
                    "StartDateTime			DATETIME NOT NULL,"
                    "EndDateTime			DATETIME NOT NULL,"
                    "CurrentIngestionsNumber	INT NOT NULL,"
                    "constraint MMS_CustomerMoreInfo_PK PRIMARY KEY (CustomerKey), "
                    "constraint MMS_CustomerMoreInfo_FK foreign key (CustomerKey) "
                        "references MMS_Customers (CustomerKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            // create table MMS_Users2
            // Type (bits: ...9876543210)
            //      bit 0: MMSAdministrator
            //      bin 1: MMSUser
            //      bit 2: EndUser
            //      bit 3: MMSEditorialUser
            //      bit 4: BillingAdministrator
            lastSQLCommand = 
                "create table if not exists MMS_Users2 ("
                    "UserKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "EMailAddress				VARCHAR (128) NULL,"
                    "Password				VARCHAR (128) NOT NULL,"
                    "UserName				VARCHAR (128) NOT NULL,"
                    "CustomerKey                            BIGINT UNSIGNED NOT NULL,"
                    "Type					INT NOT NULL,"
                    "CreationDate				TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "ExpirationDate				DATETIME NOT NULL,"
                    "constraint MMS_Users2_PK PRIMARY KEY (UserKey), "
                    "constraint MMS_Users2_FK foreign key (CustomerKey) "
                        "references MMS_Customers (CustomerKey) on delete cascade, "
                    "UNIQUE (EMailAddress))"
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create unique index MMS_Users2_idx on MMS_Users2 (CustomerKey, UserName)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_APIKeys ("
                    "APIKey                     VARCHAR (128) NOT NULL,"
                    "UserKey                    BIGINT UNSIGNED NOT NULL,"
                    "Flags			SET('ADMIN_API', 'USER_API') NOT NULL,"
                    "CreationDate		TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "ExpirationDate		DATETIME NOT NULL,"
                    "constraint MMS_APIKeys_PK PRIMARY KEY (APIKey), "
                    "constraint MMS_APIKeys_FK foreign key (UserKey) "
                        "references MMS_Users2 (UserKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_ContentProviders ("
                    "ContentProviderKey                     BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "CustomerKey                            BIGINT UNSIGNED NOT NULL,"
                    "Name					VARCHAR (64) NOT NULL,"
                    "constraint MMS_ContentProviders_PK PRIMARY KEY (ContentProviderKey), "
                    "constraint MMS_ContentProviders_FK foreign key (CustomerKey) "
                        "references MMS_Customers (CustomerKey) on delete cascade, "
                    "UNIQUE (CustomerKey, Name))" 
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            // Technology.
            //      0: Images (Download),
            //      1: 3GPP (Streaming+Download),
            //      2: MPEG2-TS (IPhone Streaming),
            //      3: WEBM (VP8 and Vorbis)
            //      4: WindowsMedia,
            //      5: Adobe
            // The encoding profile format is:
            //      [<video enc. params>][<audio enc. params>][<image enc. params>][<ringtone enc. params>][<application enc. params>][file format params]
            //  The <video enc. params> format is:
            //      _V1<video_codec>_V21<video_bitrate1>_<frame_rate_fps>_..._V2M<video_bitrateM>_<frame_rate_fps>_V3<screen_size>_V5<key_frame_interval_in_seconds>_V6<profile>
            //      <video_codec>: mpeg4, h263, h264, VP8, wmv
            //      <video_bitrate1>: 68000, 70000
            //      <screen_size>: QCIF: 176x144, SQCIF: 128x96, QSIF: 160x120, QVGA: 320x240, HVGA: 480x320, 480x360, CIF: 352x288, VGA: 640x480, SD: 720x576
            //      <frame_rate_fps>: 15, 12.5, 10, 8
            //      <key_frame_interval_in_seconds>: 3
            //      <profile>: baseline, main, high
            //  The <audio enc. params> format is:
            //      _A1<audio_codec>_A21<audio_bitrate1>_..._A2N<audio_bitrateN>_A3<Channels>_A4<sampling_rate>_
            //      <audio_codec>: amr-nb, amr-wb, aac-lc, enh-aacplus, he-aac, wma, vorbis
            //      <audio_bitrate1>: 12200
            //      <Channels>: S: Stereo, M: Mono
            //      <sampling_rate>: 8000, 11025, 32000, 44100, 48000
            //  The <image enc. params> format is:
            //	_I1<image_format>_I2<width>_I3<height>_I4<AspectRatio>_I5<Interlace>
            //	<image_format>: PNG, JPG, GIF
            //	<AspectRatio>: 1, 0
            //	<Interlace>: 0: NoInterlace, 1: LineInterlace, 2: PlaneInterlace, 3: PartitionInterlace
            //  The <ringtone enc. params> format is:
            //	...
            //  The <application enc. params> format is:
            //	...
            //  The <file format params> format is:
            //	_F1<hinter>
            //	<hinter>: 1, 0
            //  Temporary fields used by XHP: Width, Height, VideoCodec, AudioCodec
            lastSQLCommand = 
                "create table if not exists MMS_EncodingProfiles ("
                    "EncodingProfileKey  		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "ContentType			VARCHAR (32) NOT NULL,"
                    "Technology         		TINYINT NOT NULL,"
                    "Details    			VARCHAR (512) NOT NULL,"
                    "Label				VARCHAR (64) NULL,"
                    "Width				INT NOT NULL DEFAULT 0,"
                    "Height				INT NOT NULL DEFAULT 0,"
                    "VideoCodec			VARCHAR (32) null,"
                    "AudioCodec			VARCHAR (32) null,"
                    "constraint MMS_EncodingProfiles_PK PRIMARY KEY (EncodingProfileKey), "
                    "UNIQUE (ContentType, Technology, Details)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            // CustomerKey and Name
            //      both NULL: global/system EncodingProfiles for the ContentType
            //      only Name NULL: Customer default EncodingProfiles for the ContentType
            //      both different by NULL: named Customer EncodingProfiles for the ContentType
            lastSQLCommand = 
                "create table if not exists MMS_EncodingProfilesSet ("
                    "EncodingProfilesSetKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "ContentType				VARCHAR (32) NOT NULL,"
                    "CustomerKey  				BIGINT UNSIGNED NULL,"
                    "Name						VARCHAR (64) NULL,"
                    "constraint MMS_EncodingProfilesSet_PK PRIMARY KEY (EncodingProfilesSetKey)," 
                    "constraint MMS_EncodingProfilesSet_FK foreign key (CustomerKey) "
                        "references MMS_Customers (CustomerKey) on delete cascade, "
                    "UNIQUE (ContentType, CustomerKey, Name)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    

        try
        {
            //  insert global EncodingProfilesSet per ContentType
            vector<ContentType> contentTypes = { ContentType::Video, ContentType::Audio, ContentType::Image };
            
            for (ContentType contentType: contentTypes)
            {
                int     encodingProfilesSetCount = -1;
                {
                    lastSQLCommand = 
                        "select count(*) from MMS_EncodingProfilesSet where ContentType = ? and CustomerKey is NULL and Name is NULL";
                    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));

                    shared_ptr<sql::ResultSet> resultSet (preparedStatement->executeQuery());
                    if (resultSet->next())
                    {
                        encodingProfilesSetCount = resultSet->getInt(1);
                    }
                }

                if (encodingProfilesSetCount == 0)
                {
                    lastSQLCommand = 
                        "insert into MMS_EncodingProfilesSet (EncodingProfilesSetKey, ContentType, CustomerKey, Name) values (NULL, ?, NULL, NULL)";
                    shared_ptr<sql::PreparedStatement> preparedStatement (conn->_sqlConnection->prepareStatement(lastSQLCommand));
                    int queryParameterIndex = 1;
                    preparedStatement->setString(queryParameterIndex++, MMSEngineDBFacade::toString(contentType));
                    preparedStatement->executeUpdate();
                }
            }
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }    
        
        try
        {
            // EncodingProfiles associated to each family (EncodingProfilesSet)
            lastSQLCommand = 
                "create table if not exists MMS_EncodingProfilesSetMapping ("
                    "EncodingProfilesSetKey  	BIGINT UNSIGNED NOT NULL,"
                    "EncodingProfileKey			BIGINT UNSIGNED NOT NULL,"
                    "constraint MMS_EncodingProfilesSetMapping_PK PRIMARY KEY (EncodingProfilesSetKey, EncodingProfileKey), "
                    "constraint MMS_EncodingProfilesSetMapping_FK1 foreign key (EncodingProfilesSetKey) "
                        "references MMS_EncodingProfilesSet (EncodingProfilesSetKey), "
                    "constraint MMS_EncodingProfilesSetMapping_FK2 foreign key (EncodingProfileKey) "
                        "references MMS_EncodingProfiles (EncodingProfileKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            // Status.
            //  1: StartIingestion
            //  2: MetaDataSavedInDB
            //  3: MediaFileMovedInMMS
            //  8: End_IngestionFailure  # nothing done
            //  9: End_IngestionSrcSuccess_EncodingError (we will have this state if just one of the encoding failed.
            //      One encoding is considered a failure only after that the MaxFailuresNumer for this encoding is reached)
            //  10: End_IngestionSuccess  # all done
            // So, at the beginning the status is 1
            //      from 1 it could become 2 or 8 in case of failure
            //      from 2 it could become 3 or 8 in case of failure
            //      from 3 it could become 10 or 9 in case of encoding failure
            //      The final states are 8, 9 and 10
            // IngestionType could be:
            //      NULL: XML not parsed yet to know the type
            //      0: insert
            //      1: update
            //      2. remove
            // MetaDataFileName could be null to implement the following scenario done through XHP GUI:
            //      allow the user to extend the content specifying a new encoding profile to be used
            //      for a content that was already ingested previously. In this scenario we do not have
            //      any meta data file.
            lastSQLCommand = 
                "create table if not exists MMS_IngestionJobs ("
                    "IngestionJobKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "CustomerKey                BIGINT UNSIGNED NOT NULL,"
                    "MediaItemKey               BIGINT UNSIGNED NULL,"
                    "MetaDataContent            TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,"
                    "IngestionType              TINYINT (2) NULL,"
                    "StartIngestion             TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "EndIngestion               DATETIME NULL,"
                    "DownloadingProgress        DECIMAL(3,1) NULL,"
                    "UploadingProgress          DECIMAL(3,1) NULL,"
                    "SourceBinaryTransferred    INT NOT NULL,"
                    "ProcessorMMS               VARCHAR (128) NULL,"
                    "Status           			VARCHAR (64) NOT NULL,"
                    "ErrorMessage               VARCHAR (1024) NULL,"
                    "constraint MMS_IngestionJobs_PK PRIMARY KEY (IngestionJobKey), "
                    "constraint MMS_IngestionJobs_FK foreign key (CustomerKey) "
                        "references MMS_Customers (CustomerKey) on delete cascade) "	   	        				
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_Genres ("
                    "GenreKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "Name				VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,"
                    "constraint MMS_Genres_PK PRIMARY KEY (GenreKey), "
                    "UNIQUE (Name)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // CustomerKey is the owner of the content
            // ContentType: 0: video, 1: audio, 2: image, 3: application, 4: ringtone (it uses the same audio tables),
            //		5: playlist, 6: live
            // IngestedRelativePath MUST start always with '/' and ends always with '/'
            // IngestedFileName and IngestedRelativePath refer the ingested content independently
            //		if it is encoded or uncompressed
            // if EncodingProfilesSet is NULL, it means the ingested content is already encoded
            // The ContentProviderKey is the entity owner of the content. For example H3G is our customer and EMI is the ContentProvider.
            lastSQLCommand = 
                "create table if not exists MMS_MediaItems ("
                    "MediaItemKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "CustomerKey			BIGINT UNSIGNED NOT NULL,"
                    "ContentProviderKey			BIGINT UNSIGNED NOT NULL,"
                    "GenreKey				BIGINT UNSIGNED NULL,"
                    "Title      			VARCHAR (256) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,"
                    "SubTitle				MEDIUMTEXT CHARACTER SET utf8 COLLATE utf8_bin NULL,"
                    "Ingester				VARCHAR (128) NULL,"
                    "Keywords				VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NULL,"
                    "Description			TEXT CHARACTER SET utf8 COLLATE utf8_bin NULL,"
                    "IngestionDate			TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "ContentType                        VARCHAR (32) NOT NULL,"
                    "LogicalType			VARCHAR (32) NULL,"
                    "EncodingProfilesSetKey		BIGINT UNSIGNED NULL,"
                    "constraint MMS_MediaItems_PK PRIMARY KEY (MediaItemKey), "
                    "constraint MMS_MediaItems_FK foreign key (CustomerKey) "
                        "references MMS_Customers (CustomerKey) on delete cascade, "
                    "constraint MMS_MediaItems_FK2 foreign key (ContentProviderKey) "
                        "references MMS_ContentProviders (ContentProviderKey), "
                    "constraint MMS_MediaItems_FK3 foreign key (EncodingProfilesSetKey) "
                        "references MMS_EncodingProfilesSet (EncodingProfilesSetKey), "
                    "constraint MMS_MediaItems_FK4 foreign key (GenreKey) "
                        "references MMS_Genres (GenreKey)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create index MMS_MediaItems_idx2 on MMS_MediaItems (ContentType, LogicalType, IngestionDate)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create index MMS_MediaItems_idx3 on MMS_MediaItems (ContentType, IngestionDate)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create index MMS_MediaItems_idx4 on MMS_MediaItems (ContentType, Title)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // DRM. 0: NO DRM, 1: YES DRM
            // EncodedFileName and EncodedRelativePath are NULL only if the content is un-compressed.
            //  EncodedRelativePath MUST start always with '/' and ends always with '/'
            // EncodingProfileKey will be NULL only in case of
            //      - an un-compressed video or audio
            //      - an Application
            // MMSPartitionNumber. -1: live partition, >= 0: partition for any other content
            // IsAlias (0: false): it is used for a PhysicalPath that is an alias and
            //  it really refers another existing PhysicalPath. It was introduced to manage the XLE live profile
            //  supporting really multi profiles: rtsp, hls, adobe. So for every different profiles, we will
            //  create just an alias
            lastSQLCommand = 
                "create table if not exists MMS_PhysicalPaths ("
                    "PhysicalPathKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "MediaItemKey			BIGINT UNSIGNED NOT NULL,"
                    "DRM	             		TINYINT NOT NULL,"
                    "FileName				VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,"
                    "RelativePath			VARCHAR (256) NOT NULL,"
                    "MMSPartitionNumber			INT NULL,"
                    "SizeInBytes			BIGINT UNSIGNED NOT NULL,"
                    "EncodingProfileKey			BIGINT UNSIGNED NULL,"
                    "IsAlias				INT NOT NULL DEFAULT 0,"
                    "CreationDate			TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "constraint MMS_PhysicalPaths_PK PRIMARY KEY (PhysicalPathKey), "
                    "constraint MMS_PhysicalPaths_FK foreign key (MediaItemKey) "
                        "references MMS_MediaItems (MediaItemKey) on delete cascade, "
                    "constraint MMS_PhysicalPaths_FK2 foreign key (EncodingProfileKey) "
                        "references MMS_EncodingProfiles (EncodingProfileKey), "
                    "UNIQUE (MediaItemKey, RelativePath, FileName, IsAlias), "
                    "UNIQUE (MediaItemKey, EncodingProfileKey)) "	// it is not possible to have the same content using the same encoding profile key
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // that index is important for the periodical select done by 'checkPublishing'
            lastSQLCommand = 
                "create index MMS_PhysicalPaths_idx2 on MMS_PhysicalPaths (MediaItemKey, PhysicalPathKey, EncodingProfileKey, MMSPartitionNumber)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // that index is important for the periodical select done by 'checkPublishing'
            lastSQLCommand = 
                "create index MMS_PhysicalPaths_idx3 on MMS_PhysicalPaths (RelativePath, FileName)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_VideoItems ("
                    "MediaItemKey			BIGINT UNSIGNED NOT NULL,"
                    "DurationInMilliSeconds		BIGINT NULL,"
                    "constraint MMS_VideoItems_PK PRIMARY KEY (MediaItemKey), "
                    "constraint MMS_VideoItems_FK foreign key (MediaItemKey) "
                        "references MMS_MediaItems (MediaItemKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_AudioItems ("
                    "MediaItemKey			BIGINT UNSIGNED NOT NULL,"
                    "DurationInMilliSeconds		BIGINT NULL,"
                    "constraint MMS_AudioItems_PK PRIMARY KEY (MediaItemKey), "
                    "constraint MMS_AudioItems_FK foreign key (MediaItemKey) "
                        "references MMS_MediaItems (MediaItemKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            lastSQLCommand = 
                "create table if not exists MMS_ImageItems ("
                    "MediaItemKey				BIGINT UNSIGNED NOT NULL,"
                    "Width						INT NOT NULL,"
                    "Height						INT NOT NULL,"
                    "constraint MMS_ImageItems_PK PRIMARY KEY (MediaItemKey), "
                    "constraint MMS_ImageItems_FK foreign key (MediaItemKey) "
                        "references MMS_MediaItems (MediaItemKey) on delete cascade) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            // Reservecredit is not NULL only in case of PayPerEvent or Bundle. In these cases, it will be 0 or 1.
            lastSQLCommand = 
                "create table if not exists MMS_DefaultTerritoryInfo ("
                    "DefaultTerritoryInfoKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "MediaItemKey				BIGINT UNSIGNED NOT NULL,"
                    "TerritoryKey				BIGINT UNSIGNED NOT NULL,"
                    /*
                    "DownloadChargingKey1			BIGINT UNSIGNED NOT NULL,"
                    "DownloadChargingKey2			BIGINT UNSIGNED NOT NULL,"
                    "DownloadReserveCredit			TINYINT (1) NULL,"
                    "DownloadExternalBillingName		VARCHAR (64) NULL,"
                    "DownloadMaxRetries				INT NOT NULL,"
                    "DownloadTTLInSeconds			INT NOT NULL,"
                    "StreamingChargingKey1			BIGINT UNSIGNED NOT NULL,"
                    "StreamingChargingKey2			BIGINT UNSIGNED NOT NULL,"
                    "StreamingReserveCredit			TINYINT (1) NULL,"
                    "StreamingExternalBillingName		VARCHAR (64) NULL,"
                    "StreamingMaxRetries			INT NOT NULL,"
                    "StreamingTTLInSeconds			INT NOT NULL,"
                    */
                    "StartPublishing				DATETIME NOT NULL,"
                    "EndPublishing				DATETIME NOT NULL,"
                    "constraint MMS_DefaultTerritoryInfo_PK PRIMARY KEY (DefaultTerritoryInfoKey), "
                    "constraint MMS_DefaultTerritoryInfo_FK foreign key (MediaItemKey) "
                        "references MMS_MediaItems (MediaItemKey) on delete cascade, "
                    "constraint MMS_DefaultTerritoryInfo_FK2 foreign key (TerritoryKey) "
                        "references MMS_Territories (TerritoryKey) on delete cascade, "
                    /*
                    "constraint MMS_DefaultTerritoryInfo_FK3 foreign key (DownloadChargingKey1) "
                        "references ChargingInfo (ChargingKey), "
                    "constraint MMS_DefaultTerritoryInfo_FK4 foreign key (DownloadChargingKey2) "
                        "references ChargingInfo (ChargingKey), "
                    "constraint MMS_DefaultTerritoryInfo_FK5 foreign key (StreamingChargingKey1) "
                        "references ChargingInfo (ChargingKey), "
                    "constraint MMS_DefaultTerritoryInfo_FK6 foreign key (StreamingChargingKey2) "
                        "references ChargingInfo (ChargingKey), "
                     */
                    "UNIQUE (MediaItemKey, TerritoryKey)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            // PublishingStatus. 0: not published, 1: published
            // In this table it is considered the publishing 'per content'.
            // In MMS_Publishing2, a content is considered published if all his profiles are published.
            lastSQLCommand = 
                "create table if not exists MMS_Publishing2 ("
                    "PublishingKey                  BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "MediaItemKey                   BIGINT UNSIGNED NOT NULL,"
                    "TerritoryKey                   BIGINT UNSIGNED NOT NULL,"
                    "StartPublishing                DATETIME NOT NULL,"
                    "EndPublishing                  DATETIME NOT NULL,"
                    "PublishingStatus               TINYINT (1) NOT NULL,"
                    "ProcessorMMS                   VARCHAR (128) NULL,"
                    "constraint MMS_Publishing2_PK PRIMARY KEY (PublishingKey), "
                    "constraint MMS_Publishing2_FK foreign key (MediaItemKey) "
                        "references MMS_MediaItems (MediaItemKey) on delete cascade, "
                    "constraint MMS_Publishing2_FK2 foreign key (TerritoryKey) "
                        "references MMS_Territories (TerritoryKey) on delete cascade, "
                    "UNIQUE (MediaItemKey, TerritoryKey)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
        
        try
        {
            // PublishingStatus. 0: not published, 1: published
            // In this table it is considered the publishing 'per content'.
            // In MMS_Publishing2, a content is considered published if all his profiles are published.
            lastSQLCommand = 
                "create index MMS_Publishing2_idx2 on MMS_Publishing2 (MediaItemKey, StartPublishing, EndPublishing, PublishingStatus)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // PublishingStatus. 0: not published, 1: published
            // In this table it is considered the publishing 'per content'.
            // In MMS_Publishing2, a content is considered published if all his profiles are published.
            lastSQLCommand = 
                "create index MMS_Publishing2_idx3 on MMS_Publishing2 (PublishingStatus, StartPublishing, EndPublishing)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // PublishingStatus. 0: not published, 1: published
            // In this table it is considered the publishing 'per content'.
            // In MMS_Publishing2, a content is considered published if all his profiles are published.
            lastSQLCommand = 
                "create index MMS_Publishing2_idx4 on MMS_Publishing2 (PublishingStatus, EndPublishing, StartPublishing)";            
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // The MMS_EncodingJobs table include all the contents that have to be encoded
            //  OriginatingProcedure.
            //      0: ContentIngestion1_0
            //          Used fields: FileName, RelativePath, CustomerKey, PhysicalPathKey, EncodingProfileKey
            //          The other fields will be NULL
            //      1: Encoding1_0
            //          Used fields: FileName, RelativePath, CustomerKey, FTPIPAddress (optional), FTPPort (optional),
            //              FTPUser (optional), FTPPassword (optional), EncodingProfileKey
            //          The other fields will be NULL
            //  RelativePath: it is the relative path of the original uncompressed file name
            //  PhysicalPathKey: it is the physical path key of the original uncompressed file name
            //  The ContentType was added just to avoid a big join to retrieve this info
            //  ProcessorMMS is the MMSEngine processing the encoding
            //  Status.
            //      0: TOBEPROCESSED
            //      1: PROCESSING
            //      2: SUCCESS (PROCESSED)
            //      3: FAILED
            //  EncodingPriority:
            //      0: low
            //      1: default
            //      2: high
            lastSQLCommand = 
                "create table if not exists MMS_EncodingJobs ("
                    "EncodingJobKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
                    "IngestionJobKey			BIGINT UNSIGNED NOT NULL,"
                    "SourcePhysicalPathKey		BIGINT UNSIGNED NULL,"
                    "EncodingPriority			TINYINT NOT NULL,"
                    "EncodingProfileKey			BIGINT UNSIGNED NOT NULL,"
                    "EncodingJobStart			TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                    "EncodingJobEnd			DATETIME NULL,"
                    "EncodingProgress                   INT NULL,"
                    "Status           			VARCHAR (64) NOT NULL,"
                    "ProcessorMMS			VARCHAR (128) NULL,"
                    "FailuresNumber           	INT NOT NULL,"
                    "constraint MMS_EncodingJobs_PK PRIMARY KEY (EncodingJobKey), "
                    "constraint MMS_EncodingJobs_FK foreign key (IngestionJobKey) "
                        "references MMS_IngestionJobs (IngestionJobKey) on delete cascade, "
                    "constraint MMS_EncodingJobs_FK3 foreign key (SourcePhysicalPathKey) "
                    // on delete cascade is necessary because when the ingestion fails, it is important that the 'removeMediaItemMetaData'
                    //      remove the rows from this table too, otherwise we will be flooded by the errors: PartitionNumber is null
                    // The consequence is that when the PhysicalPath is removed in general, also the rows from this table will be removed
                        "references MMS_PhysicalPaths (PhysicalPathKey) on delete cascade, "
                    "constraint MMS_EncodingJobs_FK4 foreign key (EncodingProfileKey) "
                        "references MMS_EncodingProfiles (EncodingProfileKey) on delete cascade, "
                    "UNIQUE (SourcePhysicalPathKey, EncodingProfileKey)) "
                    "ENGINE=InnoDB";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }

        try
        {
            // that index is important because it will be used by the query looking every 15 seconds if there are
            // contents to be encoded
            lastSQLCommand = 
                "create index MMS_EncodingJobs_idx2 on MMS_EncodingJobs (Status, ProcessorMMS, FailuresNumber, EncodingJobStart)";
            statement->execute(lastSQLCommand);
        }
        catch(sql::SQLException se)
        {
            if (isRealDBError(se.what()))
            {
                _logger->error(__FILEREF__ + "SQL exception"
                    + ", lastSQLCommand: " + lastSQLCommand
                    + ", se.what(): " + se.what()
                );

                throw se;
            }
        }
    
    
        /*
    # create table MMS_HTTPSessions
    # One session is per UserKey and UserAgent
    create table if not exists MMS_HTTPSessions (
            HTTPSessionKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            UserKey					BIGINT UNSIGNED NOT NULL,
            UserAgent					VARCHAR (512) NOT NULL,
            CreationDate				TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            ExpirationDate				DATETIME NOT NULL,
            constraint MMS_HTTPSessions_PK PRIMARY KEY (HTTPSessionKey), 
            constraint MMS_HTTPSessions_FK foreign key (UserKey) 
                    references MMS_Users2 (UserKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_HTTPSessions_idx on MMS_HTTPSessions (UserKey, UserAgent);

    # create table MMS_ReportsConfiguration
    # Type. 0: Billing Statistics, 1: Content Access, 2: Active Users,
    #		3: Streaming Statistics, 4: Usage (how to call the one in XHP today?)
    # Period. 0: Hourly, 1: Daily, 2: Weekly, 3: Monthly, 4: Yearly
    # Format. 0: CSV, 1: HTML
    # EmailAddresses. List of email addresses separated by ‘;’
    create table if not exists MMS_ReportsConfiguration (
            ReportConfigurationKey		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            CustomerKey             	BIGINT UNSIGNED NOT NULL,
            Type						INT NOT NULL,
            Period						INT NOT NULL,
            TimeOfDay					INT NOT NULL,
            Format						INT NOT NULL,
            EmailAddresses				VARCHAR (1024) NULL,
            constraint MMS_ReportsConfiguration_PK PRIMARY KEY (ReportConfigurationKey), 
            constraint MMS_ReportsConfiguration_FK foreign key (CustomerKey) 
                    references MMS_Customers (CustomerKey) on delete cascade, 
            UNIQUE (CustomerKey, Type, Period)) 
            ENGINE=InnoDB;

    # create table MMS_ReportURLCategory
    create table if not exists MMS_ReportURLCategory (
            ReportURLCategoryKey		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            Name             			VARCHAR (128) NOT NULL,
            URLsPattern				VARCHAR (512) NOT NULL,
            ReportConfigurationKey		BIGINT UNSIGNED NOT NULL,
            constraint MMS_ReportURLCategory_PK PRIMARY KEY (ReportURLCategoryKey), 
            constraint MMS_ReportURLCategory_FK foreign key (ReportConfigurationKey) 
                    references MMS_ReportsConfiguration (ReportConfigurationKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table MMS_CustomersSharable
    create table if not exists MMS_CustomersSharable (
            CustomerKeyOwner			BIGINT UNSIGNED NOT NULL,
            CustomerKeySharable		BIGINT UNSIGNED NOT NULL,
            constraint MMS_CustomersSharable_PK PRIMARY KEY (CustomerKeyOwner, CustomerKeySharable), 
            constraint MMS_CustomersSharable_FK1 foreign key (CustomerKeyOwner) 
                    references MMS_Customers (CustomerKey) on delete cascade, 
            constraint MMS_CustomersSharable_FK2 foreign key (CustomerKeySharable) 
                    references MMS_Customers (CustomerKey) on delete cascade)
            ENGINE=InnoDB;

    # create table Handsets
    # It represent a family of handsets
    # Description is something like: +H.264, +enh-aac-plus, ...
    # FamilyType: 0: Delivery, 1: Music/Presentation (used by MMS Application Images)
    # SupportedDelivery: see above the definition for iSupportedDelivery_*
    create table if not exists MMS_HandsetsFamilies (
            HandsetFamilyKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            Description				VARCHAR (128) NOT NULL,
            FamilyType					INT NOT NULL,
            SupportedDelivery			INT NOT NULL DEFAULT 3,
            SmallSingleBannerProfileKey	BIGINT UNSIGNED NULL,
            MediumSingleBannerProfileKey	BIGINT UNSIGNED NULL,
            SmallIconProfileKey		BIGINT UNSIGNED NULL,
            MediumIconProfileKey		BIGINT UNSIGNED NULL,
            LargeIconProfileKey		BIGINT UNSIGNED NULL,
            constraint MMS_HandsetsFamilies_PK PRIMARY KEY (HandsetFamilyKey)) 
            ENGINE=InnoDB;

    # create table Handsets
    # The Model format is: <brand>_<Model>. i.e.: Nokia_N95
    # HTTPRedirectionForRTSP. 0 if supported, 0 if not supported
    # DRMMethod. 0: no DRM, 1: oma1forwardlock, 2: cfm, 3: cfm+
    # If HandsetFamilyKey is NULL, it means the handset is not connected to his family
    # SupportedNetworkCoverage. NULL: no specified, 0: 2G, 1: 2.5G, 2: 3G
    create table if not exists MMS_Handsets (
            HandsetKey  				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            HandsetFamilyKey			BIGINT UNSIGNED NULL,
            MusicHandsetFamilyKey		BIGINT UNSIGNED NULL,
            Brand						VARCHAR (32) NOT NULL,
            Model						VARCHAR (32) NOT NULL,
            Alias						VARCHAR (32) NULL,
            OperativeSystem			VARCHAR (32) NOT NULL,
            HTTPRedirectionForRTSP		TINYINT (1) NOT NULL,
            DRMMethod					TINYINT NOT NULL,
            ScreenWidth				INT NOT NULL,
            ScreenHeight				INT NOT NULL,
            ScreenDensity				INT NULL,
            SupportedNetworkCoverage	INT NULL,
            CreationDate				TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            constraint MMS_Handsets_PK PRIMARY KEY (HandsetKey), 
            constraint MMS_Handsets_FK foreign key (HandsetFamilyKey) 
                    references MMS_HandsetsFamilies (HandsetFamilyKey)  on delete cascade, 
            constraint MMS_Handsets_FK2 foreign key (MusicHandsetFamilyKey) 
                    references MMS_HandsetsFamilies (HandsetFamilyKey)  on delete cascade, 
            UNIQUE (Brand, Model)) 
            ENGINE=InnoDB;

    # create table UserAgents
    # The Model format is: <brand>_<Model>. i.e.: Nokia_N95
    create table if not exists MMS_UserAgents (
            UserAgentKey  				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            HandsetKey					BIGINT UNSIGNED NOT NULL,
            UserAgent					VARCHAR (512) NOT NULL,
            CreationDate				TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            constraint MMS_UserAgents_PK PRIMARY KEY (UserAgentKey), 
            constraint MMS_UserAgents_FK foreign key (HandsetKey) 
                    references MMS_Handsets (HandsetKey) on delete cascade, 
            UNIQUE (UserAgent)) 
            ENGINE=InnoDB;

    # create table HandsetsProfilesMapping
    # This table perform a mapping between (HandsetKey, NetworkCoverage) with EncodingProfileKey and a specified Priority
    # NetworkCoverage: 0: 2G, 1: 2.5G, 2: 3G.
    # If CustomerKey is NULL, it means the mapping is the default mapping
    # Priority: 1 (the best), 2, ...
    create table if not exists MMS_HandsetsProfilesMapping (
            HandsetProfileMappingKey	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            CustomerKey  				BIGINT UNSIGNED NULL,
            ContentType                TINYINT NOT NULL,
            HandsetFamilyKey			BIGINT UNSIGNED NOT NULL,
            NetworkCoverage			TINYINT NOT NULL,
            EncodingProfileKey			BIGINT UNSIGNED NOT NULL,
            Priority					INT NOT NULL,
            constraint MMS_HandsetsProfilesMapping_PK primary key (HandsetProfileMappingKey), 
            constraint MMS_HandsetsProfilesMapping_FK foreign key (CustomerKey) 
                    references MMS_Customers (CustomerKey) on delete cascade, 
            constraint MMS_HandsetsProfilesMapping_FK2 foreign key (HandsetFamilyKey) 
                    references MMS_HandsetsFamilies (HandsetFamilyKey) on delete cascade, 
            constraint MMS_HandsetsProfilesMapping_FK3 foreign key (EncodingProfileKey) 
                    references MMS_EncodingProfiles (EncodingProfileKey), 
            UNIQUE (CustomerKey, ContentType, HandsetFamilyKey, NetworkCoverage, EncodingProfileKey, Priority)) 
            ENGINE=InnoDB;


    # create table MMS_GenresTranslation
    create table if not exists MMS_GenresTranslation (
            TranslationKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            GenreKey 	 				BIGINT UNSIGNED NOT NULL,
            Field						VARCHAR (64) NOT NULL,
            LanguageCode				VARCHAR (16) NOT NULL,
            Translation				TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            constraint MMS_GenresTranslation_PK PRIMARY KEY (TranslationKey), 
            constraint MMS_GenresTranslation_FK foreign key (GenreKey) 
                    references MMS_Genres (GenreKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_GenresTranslation_idx on MMS_GenresTranslation (GenreKey, Field, LanguageCode);


    # create table MMS_MediaItemsTranslation
    create table if not exists MMS_MediaItemsTranslation (
            TranslationKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            MediaItemKey 	 			BIGINT UNSIGNED NOT NULL,
            Field						VARCHAR (64) NOT NULL,
            LanguageCode				VARCHAR (16) NOT NULL,
            Translation				TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            constraint MMS_MediaItemsTranslation_PK PRIMARY KEY (TranslationKey), 
            constraint MMS_MediaItemsTranslation_FK foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_MediaItemsTranslation_idx on MMS_MediaItemsTranslation (MediaItemKey, Field, LanguageCode);

    # create table MMS_GenresMediaItemsMapping
    create table if not exists MMS_GenresMediaItemsMapping (
            GenresMediaItemsMappingKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            GenreKey						BIGINT UNSIGNED NOT NULL,
            MediaItemKey					BIGINT UNSIGNED NOT NULL,
            constraint MMS_GenresMediaItemsMapping_PK PRIMARY KEY (GenresMediaItemsMappingKey), 
            constraint MMS_GenresMediaItemsMapping_FK foreign key (GenreKey) 
                    references MMS_Genres (GenreKey) on delete cascade, 
            constraint MMS_GenresMediaItemsMapping_FK2 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            UNIQUE (GenreKey, MediaItemKey))
            ENGINE=InnoDB;

    # create table MMS_MediaItemsCustomerMapping
    # CustomerType could be 0 (Owner of the content) or 1 (User of the shared content)
    # MMS_MediaItemsCustomerMapping table will contain one row for the Customer Ownerof the content and one row for each shared content
    create table if not exists MMS_MediaItemsCustomerMapping (
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            CustomerType				TINYINT NOT NULL,
            constraint MMS_MediaItemsCustomerMapping_PK PRIMARY KEY (MediaItemKey, CustomerKey), 
            constraint MMS_MediaItemsCustomerMapping_FK1 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint MMS_MediaItemsCustomerMapping_FK2 foreign key (CustomerKey) 
                    references MMS_Customers (CustomerKey) on delete cascade)
            ENGINE=InnoDB;

    # create table MMS_ExternalKeyMapping
    # CustomerKey is the owner of the content
    create table if not exists MMS_ExternalKeyMapping (
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            MediaItemKey 	 			BIGINT UNSIGNED NOT NULL,
            ExternalKey				VARCHAR (64) NOT NULL,
            constraint MMS_ExternalKeyMapping_PK PRIMARY KEY (CustomerKey, MediaItemKey), 
            constraint MMS_ExternalKeyMapping_FK foreign key (CustomerKey) 
                    references MMS_Customers (CustomerKey) on delete cascade, 
            constraint MMS_ExternalKeyMapping_FK2 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_ExternalKeyMapping_idx on MMS_ExternalKeyMapping (CustomerKey, ExternalKey);

    # create table MediaItemsRemoved
    create table if not exists MMS_MediaItemsRemoved (
            MediaItemRemovedKey  		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            DeletionDate				TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            MediaItemKey  				BIGINT UNSIGNED NOT NULL,
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            ContentProviderKey			BIGINT UNSIGNED NOT NULL,
            DisplayName				VARCHAR (256) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            Ingester					VARCHAR (128) NULL,
            Description				TEXT CHARACTER SET utf8 COLLATE utf8_bin NULL,
            Country					VARCHAR (32) NULL,
            IngestionDate				TIMESTAMP,
            ContentType                TINYINT NOT NULL,
            LogicalType				VARCHAR (32) NULL,
            IngestedFileName			VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NULL,
            IngestedRelativePath		VARCHAR (256) NULL, 
            constraint MMS_MediaItemsRemoved_PK PRIMARY KEY (MediaItemRemovedKey)) 
            ENGINE=InnoDB;

    # create table MMS_CrossReferences
    # This table will be used to set cross references between MidiaItems
    # Type could be:
    #	0: <not specified>
    #	1: IsScreenshotOfVideo
    #	2: IsImageOfAlbum
    create table if not exists MMS_CrossReferences (
            SourceMediaItemKey		BIGINT UNSIGNED NOT NULL,
            Type					TINYINT NOT NULL,
            TargetMediaItemKey		BIGINT UNSIGNED NOT NULL,
            constraint MMS_CrossReferences_PK PRIMARY KEY (SourceMediaItemKey, TargetMediaItemKey), 
            constraint MMS_CrossReferences_FK1 foreign key (SourceMediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint MMS_CrossReferences_FK2 foreign key (TargetMediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;
    create index MMS_CrossReferences_idx on MMS_CrossReferences (SourceMediaItemKey, TargetMediaItemKey);

    # create table MMS_3SWESubscriptions
    # This table will be used to set cross references between MidiaItems
    create table if not exists MMS_3SWESubscriptions (
            ThreeSWESubscriptionKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            Name						VARCHAR (64) NOT NULL,
            constraint MMS_3SWESubscriptions_PK PRIMARY KEY (ThreeSWESubscriptionKey), 
            UNIQUE (Name)) 
            ENGINE=InnoDB;

    # create table MMS_3SWESubscriptionsMapping
    # This table will be used to specified the contents to be added in an HTML presentation
    create table if not exists MMS_3SWESubscriptionsMapping (
            MediaItemKey 	 			BIGINT UNSIGNED NOT NULL,
            ThreeSWESubscriptionKey	BIGINT UNSIGNED NOT NULL,
            constraint MMS_3SWESubscriptionsMapping_PK PRIMARY KEY (MediaItemKey, ThreeSWESubscriptionKey), 
            constraint MMS_3SWESubscriptionsMapping_FK1 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint MMS_3SWESubscriptionsMapping_FK2 foreign key (ThreeSWESubscriptionKey) 
                    references MMS_3SWESubscriptions (ThreeSWESubscriptionKey) on delete cascade) 
            ENGINE=InnoDB;
    create index MMS_3SWESubscriptionsMapping_idx on MMS_3SWESubscriptionsMapping (MediaItemKey, ThreeSWESubscriptionKey);

    # create table MMS_3SWEMoreChargingInfo
    # This table include the information included into the Billing definition and
    # missing in ChargingInfo table
    create table if not exists MMS_3SWEMoreChargingInfo (
            ChargingKey				BIGINT UNSIGNED NOT NULL,
            AssetType					VARCHAR (16) NOT NULL,
            AmountTax					INT NOT NULL,
            PartnerID					VARCHAR (32) NOT NULL,
            Category					VARCHAR (32) NOT NULL,
            RetailAmount				INT NOT NULL,
            RetailAmountTax			INT NOT NULL,
            RetailAmountWithSub		INT NOT NULL,
            RetailAmountTaxWithSub		INT NOT NULL,
            constraint MMS_3SWEMoreChargingInfo_PK PRIMARY KEY (ChargingKey), 
            constraint MMS_3SWEMoreChargingInfo_FK foreign key (ChargingKey) 
                    references ChargingInfo (ChargingKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table MMS_Advertisements
    # TerritoryKey: if NULL the ads is valid for any territory
    # Type:
    #		0: pre-roll
    #		1: post-roll
    create table if not exists MMS_Advertisements (
            AdvertisementKey			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            TerritoryKey				BIGINT UNSIGNED NULL,
            Name						VARCHAR (32) NOT NULL,
            ContentType				TINYINT NOT NULL,
            IsEnabled	                TINYINT (1) NOT NULL,
            Type						TINYINT NOT NULL,
            ValidityStart				TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
            ValidityEnd				TIMESTAMP NOT NULL,
            constraint MMS_Advertisements_PK PRIMARY KEY (AdvertisementKey), 
            constraint MMS_Advertisements_FK foreign key (CustomerKey) 
                    references MMS_Customers (CustomerKey) on delete cascade, 
            constraint MMS_Advertisements_FK2 foreign key (TerritoryKey) 
                    references MMS_Territories (TerritoryKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_Advertisements_idx on MMS_Advertisements (CustomerKey, TerritoryKey, Name);

    # create table MMS_AdvertisementAdvertisings
    create table if not exists MMS_Advertisement_Ads (
            AdvertisementKey			BIGINT UNSIGNED NOT NULL,
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            constraint MMS_Advertisement_Ads_PK PRIMARY KEY (AdvertisementKey, MediaItemKey), 
            constraint MMS_Advertisement_Ads_FK foreign key (AdvertisementKey) 
                    references MMS_Advertisements (AdvertisementKey) on delete cascade, 
            constraint MMS_Advertisement_Ads_FK2 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table MMS_AdvertisementContents
    create table if not exists MMS_Advertisement_Contents (
            AdvertisementKey			BIGINT UNSIGNED NOT NULL,
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            constraint MMS_Advertisement_Contents_PK PRIMARY KEY (AdvertisementKey, MediaItemKey), 
            constraint MMS_Advertisement_Contents_FK foreign key (AdvertisementKey) 
                    references MMS_Advertisements (AdvertisementKey) on delete cascade, 
            constraint MMS_Advertisement_Contents_FK2 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;


    # create table MMS_RequestsAuthorization
    # MediaItemKey or ExternalKey cannot be both null
    # DeliveryMethod:
    #		0: download
    #		1: 3gpp streaming
    #		2: RTMP Flash Streaming
    #		3: WindowsMedia Streaming
    # SwitchingType: 0: None, 1: FCS, 2: FTS
    # NetworkCoverage. 0: 2G, 1: 2.5G, 2: 3G
    # IngestedPathName: [<live prefix>]/<customer name>/<territory name>/<relative path>/<content name>
    # ToBeContinued. 0 or 1
    # ForceHTTPRedirection. 0: HTML page, 1: HTTP Redirection
    # TimeToLive is measured in seconds
    create table if not exists MMS_RequestsAuthorization (
            RequestAuthorizationKey	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            PlayerIP					VARCHAR (16) NULL,
            TerritoryKey				BIGINT UNSIGNED NOT NULL,
            Shuffling					TINYINT NULL,
            PartyID					VARCHAR (64) NOT NULL,
            MSISDN						VARCHAR (32) NULL,
            MediaItemKey				BIGINT UNSIGNED NULL,
            ExternalKey				VARCHAR (64) NULL,
            EncodingProfileKey			BIGINT UNSIGNED NULL,
            EncodingLabel				VARCHAR (64) NULL,
            LanguageCode				VARCHAR (16) NULL,
            DeliveryMethod				TINYINT NULL,
            PreviewSeconds				INT NULL,
            SwitchingType				TINYINT NOT NULL default 0,
            ChargingKey				BIGINT UNSIGNED NULL,
            XSBTTLInSeconds			INT NULL,
            XSBMaxRetries				INT NULL,
            Sequence					VARCHAR (16) NULL,
            ToBeContinued				TINYINT NULL,
            ForceHTTPRedirection		TINYINT NULL,
            NetworkCoverage			TINYINT NULL,
            AuthorizationTimestamp		TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            TimeToLive					INT NOT NULL,
            AuthorizationType			VARCHAR (64) NULL,
            AuthorizationData			VARCHAR (128) NULL,
            constraint MMS_RequestsAuthorization_PK PRIMARY KEY (RequestAuthorizationKey), 
            constraint MMS_RequestsAuthorization_FK foreign key (CustomerKey) 
                    references MMS_Customers (CustomerKey) on delete cascade, 
            constraint MMS_RequestsAuthorization_FK2 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint MMS_RequestsAuthorization_FK3 foreign key (ChargingKey) 
                    references ChargingInfo (ChargingKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table MMS_RequestsStatistics
    # Status:
    #	- 0: Received
    #	- 1: Failed (final status)
    #	- 2: redirected (final status)
    create table if not exists MMS_RequestsStatistics (
            RequestStatisticKey		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            Status           			TINYINT (2) NOT NULL,
            ErrorMessage				VARCHAR (512) NULL,
            RedirectionURL				VARCHAR (256) NULL,
            RequestAuthorizationKey	BIGINT UNSIGNED NULL,
            CustomerKey				BIGINT UNSIGNED NULL,
            UserAgent					VARCHAR (512) NULL,
            HandsetKey					BIGINT UNSIGNED NULL,
            PartyID					VARCHAR (64) NULL,
            MSISDN						VARCHAR (32) NULL,
            PhysicalPathKey			BIGINT UNSIGNED NULL,
            AuthorizationKey			BIGINT UNSIGNED NULL,
            RequestReceivedTimestamp	TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            ElapsedSecondsToManageTheRequest	INT NULL,
            constraint MMS_RequestsStatistics_PK PRIMARY KEY (RequestStatisticKey)) 
            ENGINE=InnoDB;
    create index MMS_RequestsStatistics_idx2 on MMS_RequestsStatistics (AuthorizationKey);



    # create table MMS_MediaItemsPublishing
    # In this table it is considered the publishing 'per content'.
    # In MMS_MediaItemsPublishing, a content is considered published if all his profiles are published.
    create table if not exists MMS_MediaItemsPublishing (
            MediaItemPublishingKey		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT UNIQUE,
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            TerritoryKey				BIGINT UNSIGNED NOT NULL,
            constraint MMS_MediaItemsPublishing_PK PRIMARY KEY (TerritoryKey, MediaItemKey), 
            constraint MMS_MediaItemsPublishing_FK1 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint MMS_MediaItemsPublishing_FK2 foreign key (TerritoryKey) 
                    references MMS_Territories (TerritoryKey) on delete cascade)
            ENGINE=InnoDB;

    // Done by a Zoli music SQL script:
    //ALTER TABLE MMS_MediaItemsPublishing 
    //	ADD COLUMN AccessCount INT NOT NULL DEFAULT 0,
    //	ADD COLUMN Popularity DECIMAL(12, 2) NOT NULL DEFAULT 0,
    //	ADD COLUMN LastAccess DATETIME NOT NULL DEFAULT 0;
    //ALTER TABLE MMS_MediaItemsPublishing 
    //	ADD KEY idx_AccessCount (TerritoryKey, AccessCount),
    //	ADD KEY idx_Popularity (TerritoryKey, Popularity),
    //	ADD KEY idx_LastAccess (TerritoryKey, LastAccess);


    # create table MediaItemsBillingInfo
    # Reservecredit is not NULL only in case of PayPerEvent or Bundle. In these cases, it will be 0 or 1.
    create table if not exists MMS_MediaItemsBillingInfo (
            MediaItemsBillingInfoKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            PhysicalPathKey			BIGINT UNSIGNED NOT NULL,
            DeliveryMethod				TINYINT NOT NULL,
            TerritoryKey				BIGINT UNSIGNED NOT NULL,
            ChargingKey1				BIGINT UNSIGNED NOT NULL,
            ChargingKey2				BIGINT UNSIGNED NOT NULL,
            ReserveCredit				TINYINT (1) NULL,
            ExternalBillingName		VARCHAR (64) NULL,
            MaxRetries					INT NOT NULL,
            TTLInSeconds				INT NOT NULL,
            constraint MMS_MediaItemsBillingInfo_PK PRIMARY KEY (MediaItemsBillingInfoKey), 
            constraint MMS_MediaItemsBillingInfo_FK foreign key (PhysicalPathKey) 
                    references MMS_PhysicalPaths (PhysicalPathKey) on delete cascade, 
            constraint MMS_MediaItemsBillingInfo_FK2 foreign key (TerritoryKey) 
                    references MMS_Territories (TerritoryKey) on delete cascade, 
            constraint MMS_MediaItemsBillingInfo_FK3 foreign key (ChargingKey1) 
                    references ChargingInfo (ChargingKey), 
            constraint MMS_MediaItemsBillingInfo_FK4 foreign key (ChargingKey2) 
                    references ChargingInfo (ChargingKey), 
            UNIQUE (PhysicalPathKey, DeliveryMethod, TerritoryKey)) 
            ENGINE=InnoDB;


    # create table MMS_AllowedDeliveryMethods
    create table if not exists MMS_AllowedDeliveryMethods (
            ContentType				TINYINT NOT NULL,
            DeliveryMethod				TINYINT NOT NULL,
            constraint MMS_AllowedDeliveryMethods_PK PRIMARY KEY (ContentType, DeliveryMethod)) 
            ENGINE=InnoDB;


    # create table MMS_Artists
    create table if not exists MMS_Artists (
            ArtistKey  				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            Name						VARCHAR (255) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            Country					VARCHAR (128) NULL,
            HomePageURL				VARCHAR (256) NULL,
            constraint MMS_Artists_PK PRIMARY KEY (ArtistKey), 
            UNIQUE (Name)) 
            ENGINE=InnoDB;


    # create table MMS_ArtistsTranslation
    create table if not exists MMS_ArtistsTranslation (
            TranslationKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            ArtistKey 	 				BIGINT UNSIGNED NOT NULL,
            Field						VARCHAR (64) NOT NULL,
            LanguageCode				VARCHAR (16) NOT NULL,
            Translation				TEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            constraint MMS_ArtistsTranslation_PK PRIMARY KEY (TranslationKey), 
            constraint MMS_ArtistsTranslation_FK foreign key (ArtistKey) 
                    references MMS_Artists (ArtistKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_ArtistsTranslation_idx on MMS_ArtistsTranslation (ArtistKey, Field, LanguageCode);

    # create table MMS_CustomerCatalogMoreInfo
    # GlobalHomePage: 0 or 1 (it specifies if his home page has to be the global one or his private home page)
    # IsPublic: 0 or 1
    create table if not exists MMS_CustomerCatalogMoreInfo (
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            GlobalHomePage				INT NOT NULL,
            IsPublic					INT NOT NULL,
            CatalogImageMediaItemKey	BIGINT UNSIGNED NULL,
            HeaderIsEnabled_0					INT UNSIGNED NOT NULL,
            HeaderPresentationFunction_0		INT UNSIGNED NULL,
            HeaderFunctionParameters_0			VARCHAR (64) NULL,
            HeaderImageMediaItemKey_0			BIGINT UNSIGNED NULL,
            HeaderIsEnabled_1					INT UNSIGNED NOT NULL,
            HeaderPresentationFunction_1		INT UNSIGNED NULL,
            HeaderFunctionParameters_1			VARCHAR (64) NULL,
            HeaderImageMediaItemKey_1			BIGINT UNSIGNED NULL,
            HeaderIsEnabled_2					INT UNSIGNED NOT NULL,
            HeaderPresentationFunction_2		INT UNSIGNED NULL,
            HeaderFunctionParameters_2			VARCHAR (64) NULL,
            HeaderImageMediaItemKey_2			BIGINT UNSIGNED NULL,
            HeaderIsEnabled_3					INT UNSIGNED NOT NULL,
            HeaderPresentationFunction_3		INT UNSIGNED NULL,
            HeaderFunctionParameters_3			VARCHAR (64) NULL,
            HeaderImageMediaItemKey_3			BIGINT UNSIGNED NULL,
            HeaderIsEnabled_4					INT UNSIGNED NOT NULL,
            HeaderPresentationFunction_4		INT UNSIGNED NULL,
            HeaderFunctionParameters_4			VARCHAR (64) NULL,
            HeaderImageMediaItemKey_4			BIGINT UNSIGNED NULL,
            constraint MMS_CustomerCatalogMoreInfo_PK PRIMARY KEY (CustomerKey), 
            constraint MMS_CustomerCatalogMoreInfo_FK foreign key (CustomerKey) 
                    references MMS_Customers (CustomerKey) on delete cascade, 
            constraint MMS_CustomerCatalogMoreInfo_FK2 foreign key (CatalogImageMediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table MMS_PresentationWorkspaces
    # Name: if NULL, it is the Production Workspace
    create table if not exists MMS_PresentationWorkspaces (
            PresentationWorkspaceKey	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            CustomerKey				BIGINT UNSIGNED NOT NULL,
            Name						VARCHAR (128) NULL,
            constraint MMS_PresentationWorkspaces_PK PRIMARY KEY (PresentationWorkspaceKey), 
            constraint MMS_PresentationWorkspaces_FK foreign key (CustomerKey) 
                    references MMS_Customers (CustomerKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_PresentationWorkspaces_idx on MMS_PresentationWorkspaces (CustomerKey, Name);

    # create table MMS_PresentationItems
    # PresentationItemType: see PresentationItemType in MMSClient.h
    # NodeType:
    #	0: internal (no root type)
    #	1: MainRoot
    #	2: Root_1
    #	3: Root_2
    #	4: Root_3
    #	5: Root_4
    create table if not exists MMS_PresentationItems (
            PresentationItemKey		BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            PresentationWorkspaceKey	BIGINT UNSIGNED NOT NULL,
            ParentPresentationItemKey	BIGINT UNSIGNED NULL,
            NodeType					INT UNSIGNED NOT NULL,
            MediaItemKey				BIGINT UNSIGNED NULL,
            PresentationItemType		INT UNSIGNED NOT NULL,
            ImageMediaItemKey			BIGINT UNSIGNED NULL,
            Title						VARCHAR (128) NULL,
            SubTitle					VARCHAR (256) NULL,
            Parameter					VARCHAR (256) NULL,
            PositionIndex				INT UNSIGNED NOT NULL,
            ToolbarIsEnabled_1					INT UNSIGNED NOT NULL,
            ToolbarPresentationFunction_1		INT UNSIGNED NULL,
            ToolbarFunctionParameters_1			VARCHAR (64) NULL,
            ToolbarImageMediaItemKey_1			BIGINT UNSIGNED NULL,
            ToolbarIsEnabled_2					INT UNSIGNED NOT NULL,
            ToolbarPresentationFunction_2		INT UNSIGNED NULL,
            ToolbarFunctionParameters_2			VARCHAR (64) NULL,
            ToolbarImageMediaItemKey_2			BIGINT UNSIGNED NULL,
            ToolbarIsEnabled_3					INT UNSIGNED NOT NULL,
            ToolbarPresentationFunction_3		INT UNSIGNED NULL,
            ToolbarFunctionParameters_3			VARCHAR (64) NULL,
            ToolbarImageMediaItemKey_3			BIGINT UNSIGNED NULL,
            ToolbarIsEnabled_4					INT UNSIGNED NOT NULL,
            ToolbarPresentationFunction_4		INT UNSIGNED NULL,
            ToolbarFunctionParameters_4			VARCHAR (64) NULL,
            ToolbarImageMediaItemKey_4			BIGINT UNSIGNED NULL,
            ToolbarIsEnabled_5					INT UNSIGNED NOT NULL,
            ToolbarPresentationFunction_5		INT UNSIGNED NULL,
            ToolbarFunctionParameters_5			VARCHAR (64) NULL,
            ToolbarImageMediaItemKey_5			BIGINT UNSIGNED NULL,
            constraint MMS_PresentationItems_PK PRIMARY KEY (PresentationItemKey), 
            constraint MMS_PresentationItems_FK foreign key (PresentationWorkspaceKey) 
                    references MMS_PresentationWorkspaces (PresentationWorkspaceKey) on delete cascade, 
            constraint MMS_PresentationItems_FK2 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint MMS_PresentationItems_FK3 foreign key (ImageMediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_PresentationItems_idx on MMS_PresentationItems (ParentPresentationItemKey, PositionIndex);

    # create table MMS_Albums
    # AlbumKey: it is the PlaylistMediaItemKey
    create table if not exists MMS_Albums (
            AlbumKey  					BIGINT UNSIGNED NOT NULL,
            Version					VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NULL,
            UPC						VARCHAR (32) NOT NULL,
            Type						VARCHAR (32) NULL,
            ReleaseDate				DATETIME NULL,
            Title						VARCHAR (256) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            LabelName					VARCHAR (128) CHARACTER SET utf8 COLLATE utf8_bin NULL,
            Supplier					VARCHAR (64) NULL,
            SubSupplier				VARCHAR (64) NULL,
            PCopyright					VARCHAR (256) CHARACTER SET utf8 COLLATE utf8_bin NULL,
            Copyright					VARCHAR (256) CHARACTER SET utf8 COLLATE utf8_bin NULL,
            Explicit					VARCHAR (16) NULL,
            constraint MMS_Albums_PK PRIMARY KEY (AlbumKey), 
            constraint MMS_Albums_FK foreign key (AlbumKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_Albums_idx2 on MMS_Albums (Supplier, UPC);

    # create table MMS_ArtistsMediaItemsMapping
    # Role:
    #	- 'NOROLE' if not present in the XML
    #	- 'MAINARTIST' if compareToIgnoreCase(main artist)
    #	- 'FEATURINGARTIST' if compareToIgnoreCase(featuring artist)
    #	- any other string in upper case without any space
    create table if not exists MMS_ArtistsMediaItemsMapping (
            ArtistsMediaItemsMappingKey  	BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            ArtistKey						BIGINT UNSIGNED NOT NULL,
            MediaItemKey					BIGINT UNSIGNED NOT NULL,
            Role							VARCHAR (128) NOT NULL,
            constraint MMS_ArtistsMediaItemsMapping_PK PRIMARY KEY (ArtistsMediaItemsMappingKey), 
            constraint MMS_ArtistsMediaItemsMapping_FK foreign key (ArtistKey) 
                    references MMS_Artists (ArtistKey) on delete cascade, 
            constraint MMS_ArtistsMediaItemsMapping_FK2 foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            UNIQUE (ArtistKey, MediaItemKey, Role))
            ENGINE=InnoDB;
    create index MMS_ArtistsMediaItemsMapping_idx on MMS_ArtistsMediaItemsMapping (ArtistKey, Role, MediaItemKey);
    create index MMS_ArtistsMediaItemsMapping_idx2 on MMS_ArtistsMediaItemsMapping (MediaItemKey, Role);

    # create table MMS_ISRCMapping
    # SourceISRC: VideoItem or Ringtone -----> TargetISRC: AudioItem (Track)
    create table if not exists MMS_ISRCMapping (
            SourceISRC					VARCHAR (32) NOT NULL,
            TargetISRC					VARCHAR (32) NOT NULL,
            constraint MMS_ISRCMapping_PK PRIMARY KEY (SourceISRC, TargetISRC)) 
            ENGINE=InnoDB;
    create unique index MMS_ISRCMapping_idx on MMS_ISRCMapping (TargetISRC, SourceISRC);


    # create table MMS_SubTitlesTranslation
    create table if not exists MMS_SubTitlesTranslation (
            TranslationKey				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            MediaItemKey 	 			BIGINT UNSIGNED NOT NULL,
            Field						VARCHAR (64) NOT NULL,
            LanguageCode				VARCHAR (16) NOT NULL,
            Translation				MEDIUMTEXT CHARACTER SET utf8 COLLATE utf8_bin NOT NULL,
            constraint MMS_SubTitlesTranslation_PK PRIMARY KEY (TranslationKey), 
            constraint MMS_SubTitlesTranslation_FK foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;
    create unique index MMS_SubTitlesTranslation_idx on MMS_SubTitlesTranslation (MediaItemKey, Field, LanguageCode);


    # create table ApplicationItems
    create table if not exists MMS_ApplicationItems (
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            ReleaseDate				DATETIME NULL,
            constraint MMS_ApplicationItems_PK PRIMARY KEY (MediaItemKey), 
            constraint MMS_ApplicationItems_FK foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table MMS_ApplicationHandsetsMapping
    create table if not exists MMS_ApplicationHandsetsMapping (
            PhysicalPathKey			BIGINT UNSIGNED NOT NULL,
            HandsetKey					BIGINT UNSIGNED NOT NULL,
            JadFile					TEXT NULL,
            constraint MMS_ApplicationHandsetsMapping_PK PRIMARY KEY (PhysicalPathKey, HandsetKey), 
            constraint MMS_ApplicationHandsetsMapping_FK foreign key (PhysicalPathKey) 
                    references MMS_PhysicalPaths (PhysicalPathKey) on delete cascade, 
            constraint MMS_ApplicationHandsetsMapping_FK2 foreign key (HandsetKey) 
                    references MMS_Handsets (HandsetKey)) 
            ENGINE=InnoDB;
    create index MMS_ApplicationHandsetsMapping_idx on MMS_ApplicationHandsetsMapping (PhysicalPathKey, HandsetKey);

    # create table PlaylistItems
    # ClipType. 0 (iContentType_Video): video, 1 (iContentType_Audio): audio
    # ScheduledStartTime: used only in case of Linear Playlist (playlist of clips plus the scheduled_start_time field)
    create table if not exists MMS_PlaylistItems (
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            ClipType					INT NOT NULL,
            ScheduledStartTime			DATETIME NULL,
            constraint MMS_PlaylistItems_PK PRIMARY KEY (MediaItemKey), 
            constraint MMS_PlaylistItems_FK foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;

    # create table MMS_PlaylistClips
    # if ClipMediaItemKey is null it means the playlist item is a live and the LiveChannelURL will be initialized
    # LiveType. NULL: ClipMediaItemKey is initialized, 0: live from XAC/XLE, 1: live from the SDP file
    # ClipDuration: duration of the clip in milliseconds (NULL in case of live)
    # Seekable: 0 or 1 (NULL in case of live)
    create table if not exists MMS_PlaylistClips (
            PlaylistClipKey  			BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            PlaylistMediaItemKey		BIGINT UNSIGNED NOT NULL,
            ClipMediaItemKey			BIGINT UNSIGNED NULL,
            ClipSequence           	INT NOT NULL,
            ClipDuration           	BIGINT NULL,
            Seekable           		TINYINT NULL,
            WaitingProfileSince		DATETIME NULL,
            constraint MMS_PlaylistClips_PK PRIMARY KEY (PlaylistClipKey), 
            constraint MMS_PlaylistClips_FK foreign key (PlaylistMediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade, 
            constraint MMS_PlaylistClips_FK2 foreign key (ClipMediaItemKey) 
                    references MMS_MediaItems (MediaItemKey)) 
            ENGINE=InnoDB;
    create unique index MMS_PlaylistClips_idx2 on MMS_PlaylistClips (PlaylistMediaItemKey, ClipSequence);

    # create table LiveItems
    # FeedType. 0: video, 1: audio
    create table if not exists MMS_LiveItems (
            MediaItemKey				BIGINT UNSIGNED NOT NULL,
            ReleaseDate				DATETIME NULL,
            FeedType					INT NOT NULL,
            constraint MMS_LiveItems_PK PRIMARY KEY (MediaItemKey), 
            constraint MMS_LiveItems_FK foreign key (MediaItemKey) 
                    references MMS_MediaItems (MediaItemKey) on delete cascade) 
            ENGINE=InnoDB;


    # create table MimeTypes
    # HandsetBrandPattern, HandsetModelPattern and HandsetOperativeSystem must be all different from null or all equal to null
    #		we cannot have i.e. HandsetBrandPattern == null and HandsetModelPattern != null
    create table if not exists MMS_MimeTypes (
            MimeTypeKey  				BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
            MimeType					VARCHAR (64) NOT NULL,
            ContentType                TINYINT NOT NULL,
            HandsetBrandPattern		VARCHAR (32) NULL,
            HandsetModelPattern		VARCHAR (32) NULL,
            HandsetOperativeSystem		VARCHAR (32) NULL,
            EncodingProfileNamePattern	VARCHAR (64) NOT NULL,
            Description             	VARCHAR (64) NULL,
            constraint MMS_MimeTypes_PK PRIMARY KEY (MimeTypeKey)) 
            ENGINE=InnoDB;
         */

        _connectionPool->unborrow(conn);
    }
    catch(sql::SQLException se)
    {
        _connectionPool->unborrow(conn);

        _logger->error(__FILEREF__ + "SQL exception"
            + ", lastSQLCommand: " + lastSQLCommand
            + ", se.what(): " + se.what()
        );
    }    
}

bool MMSEngineDBFacade::isRealDBError(string exceptionMessage)
{        
    transform(
            exceptionMessage.begin(), 
            exceptionMessage.end(), 
            exceptionMessage.begin(), 
            [](unsigned char c){return tolower(c); } );

    if (exceptionMessage.find("already exists") == string::npos &&            // error (table) on mysql
            exceptionMessage.find("duplicate key name") == string::npos    // error (index) on mysql
            )
    {
        return true;
    }
    else
    {
        return false;
    }
}