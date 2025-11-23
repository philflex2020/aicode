curl -X POST http://localhost:8902/api/variables \
  -H "Content-Type: application/json" \
  -d '{
    "name": "test_manual_sync23456",
    "display_name": "Test Manual Sync",
    "category": "oper",
    "locator": "system.global"
  }'


