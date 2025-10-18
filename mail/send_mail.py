import smtplib 
import ssl
port = 465  # For SSL
password = "zoadgrtyqgoxxfko"
context = ssl.create_default_context()

with smtplib.SMTP_SSL("smtp.gmail.com", port, context=context) as server:
    server.login("sdcsiot@gmail.com", password)
    server.sendmail("sdcsiot@gmail.com", "sysdcs@gmail.com", " Hi buddy")
#phil:email_test$ cat new_mail_test.py 
import smtplib 
import ssl
port = 465  # For SSL
password = "zoadgrtyqgoxxfko"
context = ssl.create_default_context()

with smtplib.SMTP_SSL("smtp.gmail.com", port, context=context) as server:
    server.login("sdcsiot@gmail.com", password)
    server.sendmail("sdcsiot@gmail.com", "sysdcs@gmail.com", " Hi buddy")
    
