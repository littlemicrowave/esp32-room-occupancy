import aiocoap
import aiocoap.resource as resource
import aiocoap.error as error
import asyncio
import socket
import struct
import sqlite3 as sql
import os

DATA_PAYLOAD_FMT = "<HHfffffff"
PREDICTION_PAYLOAD_FMT = "<fi"
IPADDR = socket.gethostbyname(socket.gethostname())
latest_prediction = {}

def connect_to_db(dbname: str):
    script_dir = os.path.dirname(os.path.abspath(__file__))
    db_loc = os.path.join(script_dir, dbname)
    print("Database location: " + db_loc)
    conn = sql.connect(db_loc, timeout=10)
    cursor = conn.cursor()
    create_db = """
    CREATE TABLE IF NOT EXISTS sensor_data (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        co2_ppm FLOAT,
        tvoc_ppm FLOAT,
        bmp280_temperature FLOAT,
        bmp280_pressure FLOAT,
        mlx_object_temperature FLOAT,
        mlx_ambient_temperature FLOAT,
        humidity_dht FLOAT,
        temperature_dht FLOAT,
        pir_uptime FLOAT,
        timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
    )"""
    cursor.execute(create_db)
    cursor.close()
    conn.commit()
    return conn

class Data(resource.Resource):

    def __init__(self, conn):
        super().__init__()
        self.conn = conn

    async def render_post(self, request):
        try:
            data = tuple(round(measurement, 2) for measurement in struct.unpack_from(DATA_PAYLOAD_FMT, request.payload))
            print("Received data:", data)
            query = f"""INSERT INTO sensor_data
            (   co2_ppm,
                tvoc_ppm, 
                bmp280_temperature, 
                bmp280_pressure, 
                mlx_object_temperature, 
                mlx_ambient_temperature, 
                humidity_dht, 
                temperature_dht,
                pir_uptime  )
            VALUES {data}"""
            cursor = self.conn.cursor()
            cursor.execute(query)
            cursor.execute("SELECT * FROM sensor_data")
            db_view = cursor.fetchall()
            for row in db_view:
                print(row)
            self.conn.commit()
            return aiocoap.Message(code=aiocoap.CHANGED)
        except Exception as e:
            print(e)
            raise error.NotAcceptable("Payload was not accepted.")
        finally:
            cursor.close()
    
class Predictions(resource.Resource):
        async def render_post(self, request):
            try:
                data = tuple(round(pred, 2) for pred in struct.unpack_from(PREDICTION_PAYLOAD_FMT, request.payload))
                print("Received data: ", data)
                latest_prediction["human_count"] = data[0]
                latest_prediction["ventilation_state"] = data[1]
                return aiocoap.Message(code=aiocoap.CHANGED)
            except Exception as e:
                print(e)
                raise error.NotAcceptable("Payload was not accepted.")

async def main():
    conn = connect_to_db("data.db")
    root = resource.Site()
    root.add_resource(['data'], Data(conn))
    root.add_resource(['predictions'], Predictions())
    await aiocoap.Context.create_server_context(root, bind=(IPADDR, 5683))
    print(f"CoAP Server running on coap://{IPADDR}:5683")
    await asyncio.get_running_loop().create_future()

#loop = asyncio.get_event_loop()
#loop.create_task(main())
#loop.run_forever()

asyncio.run(main())
