import json
import datetime

test = {
        "key_1": "Value_1",
        "key_2": 10,
        "key_3": ["list_" + str(i) for i in range(5)],
        "key_4": {"nestkey_" + str(i) : "nestvalue_" + str(i) for i in range(5) },
        "key_5": datetime.datetime(1970, 1, 1, 8, 0, 0),
        datetime.datetime(1970, 1, 1, 8, 0, 0): "datetime_key"
}

class DateTimeEncoder(json.JSONEncoder):

    def _preprocess_date(self, obj):
        if isinstance(obj, (datetime.date, datetime.datetime, datetime.timedelta)):
            return str(obj)
        elif isinstance(obj, dict):
            return {self._preprocess_date(k): self._preprocess_date(v) for k,v in obj.items()}
        elif isinstance(obj, list):
            return [self._preprocess_date(i) for i in obj]
        return obj

    def default(self, obj):
        if isinstance(obj, (datetime.date, datetime.datetime, datetime.timedelta)):
            return str(obj)
        return super().default(obj)

    def iterencode(self, obj, **kwargs):  # Modified to accept and ignore additional kwargs
        return super(DateTimeEncoder, self).iterencode(self._preprocess_date(obj), **kwargs)


    # def iterencode(self, obj):
    #     return super().iterencode(self._preprocess_date(obj))

with open('output.json', 'w') as fo:
    json.dump(test, fo, cls=DateTimeEncoder)


res = json.dumps(test, cls=DateTimeEncoder, indent=4)
print(res)
