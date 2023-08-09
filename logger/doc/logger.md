initial thoughts on site_controller logger.
one of the Super Logger features is that it can retain all log messages but only notify the console or file of a filtered occurrence of these messages.
We want to know something is happening but do not want the details.
https://github.com/flexgen-power/site_controller/blob/dev/src/Logger.cpp#L184
the update_records logger function checks for a string match in the log message and rejects duplicates. No retention of rejected messages.
These may be important when looking at the log in more detail.
     we can fix the Logger to retain the unreported messages.
 A string comparison is used to detect duplicate messages and the map used to detect the duplicates used a find on a dict for the message.
Super Logger concept is to use a finer grain identification for the log message regardless of the actual data content.
It is proposed that a Log Id and Instance Id are used to define the log message rather than the actual log message string.
When comms go wrong, as we have already seen copious error messages can be produced all of which may be important. Data messages may also be produced at a high rate.
Super logger allows us to filter repeat log messages based in Log Id and Instance Id to allow an overview of the failure.
The current logger can be refactored to allow Log Id and Instance Id.
Since simple integer Ids are used the mapping will not require a time consuming text search for duplicates in  the log records.
The current logger does not allow a "tell me once" and then tell me again in the 10th time I see this.
But it could be extended to do this.
The more "off the wall" feature of Super  Logger  was vectored towards a  future state where we can rapidly collect data items and detect failures without having to recover the data from log messages.
I proposed separating the log format from the log data values.
I can , reluctantly, put that back in my toolbox if we don't see the need right now or ever.
We have to refactor the Logger.cpp system anyway since it is hard coded to work for Site Controller  it would be  easy to abstract those hard coded components into either setup code or config.
Logger.cpp uses cJSON to decode.
No problem ,  I think the main gcom_config can simply refer to the "logger_config_file" which can be common to all gcom instances and we'll set up the unique lognames in the init function.
The main questions here are:
NOTE all of these can be built as "extensions" and will not affect the current code API.
1/ Do you want to enhance the logger to use Log Id and Inst Id keys to allow a better way of detecting repeats.
2/ Save all records or a finite number (from config) even the duplicates
3/ Reset the duplicate detection ( tell me again if this is still happening)
4/ Add the tell me again if it happens more than 10 (config value) times
5/ Have an option of  a detail file, with all duplicate records, in addition to the console and file outputs.
6/ Remove hard coded log names  and leave these as config or setup  options.
I feel strongly about the use of Log Id and Inst Id as means to define a logging event. the message string can be formatted as usual. (edited) 

looking more into it we an still use the logId as a lookup for the format string. mimimal code changes for that.





https://github.com/flexgen-power/site_controller/blob/dev/include/Logger.h : line 57
template <class ... Types>
    std::string msg_string(int  LogId, int InstId, const Types&... args)
obviously keep the old API and add the new one.

Typical Logger output.
This is a bit developer foccussed I think

{"time": "05:55:38", "date": "08/09/23", "level": "info",
        "File": "Logger.cpp", "Func": "Init", "Line": 93,
        "MSG": "Setting up logger with config from: NULL"}
{"time": "05:55:38", "date": "08/09/23", "level": "error",
        "File": "Logger.cpp", "Func": "Init", "Line": 95,
        "MSG": "no logger configuration provided, default values used"}
{"time": "05:55:38", "date": "08/09/23", "level": "info",
        "File": "main.cpp", "Func": "main", "Line": 23,
        "MSG": "Some Info  argc 1."}
{"time": "05:55:38", "date": "08/09/23", "level": "warning",
        "File": "main.cpp", "Func": "main", "Line": 24,
        "MSG": "A Warning [build/main]."}
{"time": "05:55:38", "date": "08/09/23", "level": "error",
        "File": "main.cpp", "Func": "main", "Line": 25,
        "MSG": "An Error."}