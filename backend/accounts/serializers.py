from rest_framework import serializers

from .models import User


class PerfilSerializer(serializers.ModelSerializer):
    class Meta:
        model = User
        fields = ["id", "username", "email", "rol", "first_name", "last_name"]
        read_only_fields = fields
